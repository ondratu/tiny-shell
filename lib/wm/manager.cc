#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iterator>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <X11/cursorfont.h>
#include <X11/Xatom.h>

#include "wm/shell_theme.h"
#include "object.h"
#include "spawn.h"
#include "wm/manager.h"
#include "wm/options.h"
#include "wm/system_info.h"
#include "wm/client.h"
#include "x_util.h"

namespace tiny {
extern Handlers handlers;
extern std::shared_ptr<Display> display;
} // namespace tiny

namespace {

//! The self-pipe: a signal handler may only write to it, the loop reads it
int quit_pipe[2] = {-1, -1};

void on_quit_signal(int /*signal*/)
{
    const int saved_errno = errno;
    const char byte = 1;
    if (write(quit_pipe[1], &byte, 1) < 0) {
        // the pipe is full, so the loop is going to wake up anyway
    }
    errno = saved_errno;
}

const int quit_signals[] = {SIGTERM, SIGHUP, SIGINT};

//! What the signals did before, to give it back (nohup ignores SIGHUP)
struct sigaction old_actions[sizeof(quit_signals) / sizeof(int)];

} // namespace

namespace tiny::wm {

Manager::Manager(::Display* display_arg, ::Window root_arg):

    display(display_arg),
    root(root_arg),

    running(true)
{
    tiny::reap_children();
    catch_quit_signals();
    XSetWindowBackground(display_arg, root_arg, shell_theme.root_background);
    XSync(display_arg, True);

    XDefineCursor(display_arg, root_arg,
                  XCreateFontCursor(display_arg, XC_arrow));

    ::Window rv_root;
    ::Window rv_parent;
    ::Window* rv_child;
    uint32_t rv_ccount;

    // Realized (but not mapped, so they are not taken as client windows)
    // before the existing windows are added to the dock.
    XWindowAttributes attrs;
    XGetWindowAttributes(display_arg, root_arg, &attrs);
    screen_width = attrs.width;
    screen_height = attrs.height;

    wm_panel.set_icons(&wm_icons);
    wm_panel.realize(root_arg, 0, 0);
    wm_panel.set_events();
    publish_panel_strut();

    wm_dock.set_icons(&wm_icons);
    wm_dock.realize(root_arg, attrs.width / 2,
                    attrs.height - wm_dock.get_height() -
                            shell_theme.dock_border);
    wm_dock.set_events(LeaveWindowMask);
    // the strip at the bottom of the screen that shows a hidden dock
    wm_dock_reveal.realize(root_arg, 0, attrs.height - shell_theme.dock_reveal);
    wm_dock_reveal.resize(attrs.width, shell_theme.dock_reveal);
    wm_dock_reveal.set_events();
    wm_dock_reveal.on_reach.connect([this](const XEvent&) {
        dock_revealed = true;
        update_dock_visibility();
        restack_layers();
    });
    Object::connect_window(
            LeaveNotify, wm_dock.get_window(), [this](const XEvent& e) {
                // moving between the icons of the dock is not leaving it
                // (the grab of a menu is not leaving it either)
                if (e.xcrossing.detail != NotifyInferior &&
                    e.xcrossing.mode != NotifyGrab &&
                    e.xcrossing.mode != NotifyUngrab && dock_revealed) {
                    dock_revealed = false;
                    update_dock_visibility();
                }
            });

    wm_run.realize(root_arg, 0, 0);
    wm_run.set_events();
    wm_about.realize(root_arg, 0, 0);
    wm_about.set_events();

    wm_unresponsive.realize(root_arg);
    wm_unresponsive.on_wait.connect([this](const XEvent&) {
        if (asked) {
            asked->postpone_ask(Window::Clock::now());
        }
        close_unresponsive_dialog();
    });
    wm_unresponsive.on_quit.connect([this](const XEvent&) {
        Window* window = asked;
        close_unresponsive_dialog();
        if (window) {
            window->kill_client();
        }
    });

    wm_launcher.set_icons(&wm_icons);
    wm_launcher.realize(root_arg, 0, 0);
    wm_launcher.set_events();
    wm_launcher.keep_above(&wm_panel);
    wm_launcher.keep_above(&wm_dock);

    update_work_area(); // before the first window is placed

    XGrabServer(display_arg); // lock the X server for new events
    XQueryTree(display_arg, root_arg, &rv_root, &rv_parent, &rv_child,
               &rv_ccount);
    if (rv_ccount) {
        rv_ccount -= 1;
        XWindowAttributes child_attrs;

        for (uint32_t i = 0; i <= rv_ccount; ++i) {
            XGetWindowAttributes(display_arg, rv_child[i], &child_attrs);
            if (child_attrs.map_state == IsUnmapped) {
                continue; // will be mapped later
            }
            if (child_attrs.override_redirect) {
                continue; // menu, special tools (idesk) and so on
            }

            manage(rv_child[i], child_attrs, false);
        }
        XFree(rv_child);
    }

    if (!wm_tops.empty()) {
        wm_tops.back()->set_focus(); // the topmost one
    }

    is_button_down = [this]() {
        ::Window pointer_root;
        ::Window child;
        int x;
        int y;
        unsigned int buttons = 0;
        XQueryPointer(display, root, &pointer_root, &child, &x, &y, &x, &y,
                      &buttons);
        return (buttons & (Button1Mask | Button2Mask | Button3Mask |
                           Button4Mask | Button5Mask)) != 0;
    };
    set_supported();
    publish_desktops();
    publish_clients();

    wm_panel.map_all();
    wm_dock.map_all();
    wm_dock_reveal.map();
    update_dock_visibility();

    wm_dock.on_launch = [this](const std::string& id) {
        wm_launcher.hide();
        wm_launcher.launch(id);
    };
    wm_dock.list_actions = [this](const std::string& id) {
        return wm_launcher.action_names(id);
    };
    wm_dock.on_action = [this](const std::string& id, size_t index) {
        wm_launcher.hide();
        wm_launcher.launch_action(id, index);
    };
    wm_dock.on_toggle_favorite = [this](const std::string& id) {
        toggle_favorite(id);
    };
    wm_dock.on_move_favorite = [this](const std::string& id, size_t place) {
        move_favorite(id, place);
    };
    // the same menu as the icon of the dock has, over the launcher
    wm_launcher.on_menu = [this](const std::string& id,
                                 const XButtonEvent& be) {
        wm_dock.popup_menu(id, be.x_root, be.y_root);
    };
    wm_launcher.add_nested(&wm_dock.get_menu());
    wm_launcher.add_nested(&wm_dock.get_windows_menu());
    wm_dock.on_menu_closed = [this] {
        wm_launcher.refocus();
        update_dock_visibility(); // a menu kept the dock shown
    };
    register_actions();
    wm_dock.get_menu_button().on_click.connect(
            [this](const XEvent&) { actions.run("launcher"); });
    // an item hides the menu first, so the launcher opens over the desktop
    auto connect_item = [this](tiny::LabelButton& item, const char* action) {
        item.on_click.connect([this, action](const XEvent&) {
            wm_panel.menu_main.unmap();
            actions.run(action);
        });
    };
    MainMenu& menu = wm_panel.menu_main;
    connect_item(menu.about, "about");
    connect_item(menu.settings, "settings");
    connect_item(menu.applications, "applications");
    connect_item(menu.terminal, "terminal");
    connect_item(menu.force_quit, "force-quit");
    connect_item(menu.restart, "restart");
    connect_item(menu.shutdown, "shutdown");
    connect_item(menu.logout, "logout");

    XClearWindow(display_arg, root_arg);
    set_events();
    XSetErrorHandler(&Manager::on_error);
    XUngrabServer(display_arg); // unlock the X server
}

Manager::~Manager()
{
    // Alt + Tab
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_Tab), Mod1Mask,
                       root);
    // Shift + Alt + Tab
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_Tab),
                       ShiftMask | Mod1Mask, root);
    // Alt + F2
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_F2), Mod1Mask,
                       root);
    grab_super(false);
    stop_move_resize(false);
    release_windows();
    clear_published();
    release_quit_signals();

    if (old_error_handler) {
        XSetErrorHandler(old_error_handler);
    }
    // the display belongs to the caller, only stop managing the root window
    XSelectInput(display, root, 0);
}

void Manager::register_actions()
{
    actions.add("about",
                [this] { wm_about.popup(version_number(), system_info()); });
    actions.add("settings", [this] { run_program({settings_command}); });
    actions.add("terminal", [this] { run_program(terminal_program); });
    actions.add("force-quit", [this] { run_program({force_quit_program}); });
    actions.add("restart", [this] { run_system_command(restart_command); });
    actions.add("shutdown", [this] { run_system_command(shutdown_command); });
    actions.add("logout", [this] { running = false; });
    actions.add("applications", [this] {
        if (!wm_launcher.is_open()) { // the item opens it, never closes it
            wm_launcher.popup();
        }
    });
    actions.add("launcher", [this] { wm_launcher.toggle(); });
    actions.add("run-dialog", [this] { wm_run.popup(); });
    actions.add("next-window", [this] { activate_next_window(); });
    actions.add("prev-window", [this] { activate_prev_window(); });
    actions.add("theme-dark", [this] { set_variant(Variant::Dark); });
    actions.add("theme-light", [this] { set_variant(Variant::Light); });
    actions.add("theme-toggle", [this] {
        set_variant(tiny::theme.get_variant() == Variant::Dark ? Variant::Light
                                                               : Variant::Dark);
    });
}

void Manager::set_variant(Variant variant)
{
    shell_theme.set_variant(variant);
    if (wallpaper_command.empty()) { // a picture does not change with the theme
        XSetWindowBackground(display, root, shell_theme.root_background);
        XClearWindow(display, root);
    }
    wm_panel.restyle();
    wm_panel.menu_main.restyle();
    wm_dock.restyle();
    wm_dock.get_menu().restyle();
    wm_dock.get_windows_menu().restyle();
    wm_launcher.restyle();
    wm_run.restyle();
    wm_about.restyle();
    wm_unresponsive.restyle();
    for (const auto& entry : wm_windows) {
        entry.second->restyle_frame();
    }
}

void Manager::set_events()
{
    // StructureNotify: the size of the screen (RandR) changes
    XSelectInput(display, root,
                 SubstructureNotifyMask | SubstructureRedirectMask |
                         StructureNotifyMask);

    // Alt + Tab
    tiny::x_grab_key(display, XKeysymToKeycode(display, XK_Tab), Mod1Mask,
                     root);
    // Shift + Alt + Tab
    tiny::x_grab_key(display, XKeysymToKeycode(display, XK_Tab),
                     ShiftMask | Mod1Mask, root);
    // Alt + F2
    tiny::x_grab_key(display, XKeysymToKeycode(display, XK_F2), Mod1Mask, root);
    grab_super(true);
}

void Manager::grab_super(bool grab)
{
    // Super alone opens the launcher, so a shortcut with Super is not passed
    // to an application
    const KeySym supers[] = {XK_Super_L, XK_Super_R};
    for (const KeySym super : supers) {
        const KeyCode keycode = XKeysymToKeycode(display, super);
        if (keycode == 0) { // the keyboard has no such key
            continue;
        }
        grab ? tiny::x_grab_key(display, keycode, 0, root)
             : tiny::x_ungrab_key(display, keycode, 0, root);
    }
}

void Manager::set_supported()
{
    const tiny::Display& dsp = tiny::get_display();

    // only what is really handled, applications trust this list
    const Atom supported[] = {dsp._NET_SUPPORTED,
                              dsp._NET_SUPPORTING_WM_CHECK,
                              dsp._NET_CLIENT_LIST,
                              dsp._NET_CLIENT_LIST_STACKING,
                              dsp._NET_ACTIVE_WINDOW,
                              dsp._NET_CLOSE_WINDOW,
                              dsp._NET_WM_PING,
                              dsp._NET_WM_MOVERESIZE,
                              dsp._NET_NUMBER_OF_DESKTOPS,
                              dsp._NET_CURRENT_DESKTOP,
                              dsp._NET_DESKTOP_GEOMETRY,
                              dsp._NET_DESKTOP_VIEWPORT,
                              dsp._NET_WM_DESKTOP,
                              dsp._NET_WM_ALLOWED_ACTIONS,
                              dsp._NET_WM_ACTION_MOVE,
                              dsp._NET_WM_ACTION_RESIZE,
                              dsp._NET_WM_ACTION_MINIMIZE,
                              dsp._NET_WM_ACTION_SHADE,
                              dsp._NET_WM_ACTION_STICK,
                              dsp._NET_WM_ACTION_MAXIMIZE_HORZ,
                              dsp._NET_WM_ACTION_MAXIMIZE_VERT,
                              dsp._NET_WM_ACTION_FULLSCREEN,
                              dsp._NET_WM_ACTION_CHANGE_DESKTOP,
                              dsp._NET_WM_ACTION_CLOSE,
                              dsp._NET_WM_ACTION_ABOVE,
                              dsp._NET_WM_ACTION_BELOW,
                              dsp._NET_WM_STATE_STICKY,
                              dsp._NET_WM_STATE_SKIP_PAGER,
                              dsp._NET_FRAME_EXTENTS,
                              dsp._NET_REQUEST_FRAME_EXTENTS,
                              dsp._NET_WORKAREA,
                              dsp._NET_WM_NAME,
                              dsp._NET_WM_WINDOW_TYPE,
                              dsp._NET_WM_STATE,
                              dsp._NET_WM_STATE_MAXIMIZED_VERT,
                              dsp._NET_WM_STATE_MAXIMIZED_HORZ,
                              dsp._NET_WM_STATE_FULLSCREEN,
                              dsp._NET_WM_STATE_HIDDEN,
                              dsp._NET_WM_STATE_FOCUSED,
                              dsp._NET_WM_STATE_SKIP_TASKBAR,
                              dsp._NET_WM_STATE_ABOVE,
                              dsp._NET_WM_STATE_BELOW,
                              dsp._NET_WM_STATE_DEMANDS_ATTENTION,
                              dsp._NET_WM_STATE_MODAL,
                              dsp._NET_WM_STRUT,
                              dsp._NET_WM_STRUT_PARTIAL};
    XChangeProperty(display, root, dsp._NET_SUPPORTED, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(supported),
                    sizeof(supported) / sizeof(Atom));

    if (check_window == None) {
        check_window =
                XCreateSimpleWindow(display, root, -1, -1, 1, 1, 0, 0, 0);
    }
    static const char name[] = "tiny-shell";
    XChangeProperty(display, check_window, dsp._NET_WM_NAME, dsp.UTF8_STRING, 8,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(name),
                    sizeof(name) - 1);
    for (const ::Window w : {root, check_window}) {
        XChangeProperty(display, w, dsp._NET_SUPPORTING_WM_CHECK, XA_WINDOW, 32,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&check_window),
                        1);
    }
}

void Manager::publish_desktops()
{
    const tiny::Display& dsp = tiny::get_display();
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);
    const long one = 1;
    const long first = 0;
    const long geometry[] = {attrs.width, attrs.height};
    const long viewport[] = {0, 0};
    const struct {
        Atom property;
        const long* values = nullptr;
        int count = 0;
    } properties[] = {{dsp._NET_NUMBER_OF_DESKTOPS, &one, 1},
                      {dsp._NET_CURRENT_DESKTOP, &first, 1},
                      {dsp._NET_DESKTOP_GEOMETRY, geometry, 2},
                      {dsp._NET_DESKTOP_VIEWPORT, viewport, 2}};
    for (const auto& p : properties) {
        XChangeProperty(
                display, root, p.property, XA_CARDINAL, 32, PropModeReplace,
                reinterpret_cast<const unsigned char*>(p.values), p.count);
    }
}

void Manager::clear_published()
{
    const tiny::Display& dsp = tiny::get_display();
    for (const Atom property :
         {dsp._NET_SUPPORTED, dsp._NET_SUPPORTING_WM_CHECK,
          dsp._NET_CLIENT_LIST, dsp._NET_CLIENT_LIST_STACKING,
          dsp._NET_ACTIVE_WINDOW, dsp._NET_NUMBER_OF_DESKTOPS,
          dsp._NET_CURRENT_DESKTOP, dsp._NET_DESKTOP_GEOMETRY,
          dsp._NET_DESKTOP_VIEWPORT}) {
        XDeleteProperty(display, root, property);
    }
    if (check_window != None) {
        XDestroyWindow(display, check_window);
        check_window = None;
    }
}

void Manager::publish_clients()
{
    const tiny::Display& dsp = tiny::get_display();

    std::vector<::Window> clients;
    clients.reserve(wm_tops.size());
    for (const Window* window : wm_tops) {
        clients.push_back(window->get_child());
    }
    XChangeProperty(display, root, dsp._NET_CLIENT_LIST, XA_WINDOW, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(clients.data()),
                    static_cast<int>(clients.size()));

    publish_stacking();

    // a minimized window is not the active one for the others
    const ::Window current =
            active && !active->get_minimized() ? active->get_child() : None;
    XChangeProperty(display, root, dsp._NET_ACTIVE_WINDOW, XA_WINDOW, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&current), 1);
}

void Manager::publish_stacking()
{
    const tiny::Display& dsp = tiny::get_display();

    // the frames lie on the root in the stacking order, the lowest first
    ::Window root_return;
    ::Window parent_return;
    ::Window* children = nullptr;
    unsigned int count = 0;
    std::vector<::Window> clients;
    clients.reserve(wm_tops.size());
    if (XQueryTree(display, root, &root_return, &parent_return, &children,
                   &count)) {
        for (unsigned int i = 0; i < count; ++i) {
            for (const Window* window : wm_tops) {
                if (window->frame() == children[i]) {
                    clients.push_back(window->get_child());
                    break;
                }
            }
        }
    }
    if (children) {
        XFree(children);
    }
    XChangeProperty(display, root, dsp._NET_CLIENT_LIST_STACKING, XA_WINDOW, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(clients.data()),
                    static_cast<int>(clients.size()));
}

void Manager::restack_layers()
{
    // desktop, below, normal, panels and docks, above, fullscreen: the layers
    // from the bottom; the last one raised is on top of the others
    for (Window* window : wm_tops) {
        if (window->is_below() && !window->get_minimized()) {
            for (Window* dialog : transients_of(window)) {
                dialog->lower(); // a dialog stays with its parent, over it
            }
            window->lower();
        }
    }
    for (const ::Window desktop : wm_desktops) { // under all the others
        XLowerWindow(display, desktop);
    }
    XRaiseWindow(display, wm_panel.get_window());
    XRaiseWindow(display, wm_dock.get_window());
    for (const auto& dock : wm_docks) {
        XRaiseWindow(display, dock.first);
    }
    // the focused window last, so it stays on top of the others in its layer
    const auto raise_if = [this](Window* window, bool layer) {
        if (layer && !window->get_minimized()) {
            window->raise();
            for (Window* dialog : transients_of(window)) {
                dialog->raise(); // a dialog stays over its parent
            }
        }
    };
    const auto raise_layer = [&](bool (*in_layer)(const Window*)) {
        for (Window* window : wm_tops) {
            if (window != active) {
                raise_if(window, in_layer(window));
            }
        }
        if (active) {
            raise_if(active, in_layer(active));
        }
    };
    raise_layer([](const Window* w) { return w->is_above(); });
    raise_layer([](const Window* w) { return w->is_fullscreen(); });
    XRaiseWindow(display, wm_run.get_window()); // hidden until it is needed
    XRaiseWindow(display, wm_launcher.get_window());
    if (asked) {
        wm_unresponsive.place_over(asked->frame()); // follows its window
    }
    // over everything, also over a maximized or full screen window, or the
    // pointer at the edge of the screen would not reach it
    XRaiseWindow(display, wm_dock_reveal.get_window());
}

void Manager::request_frame_extents(::Window child)
{
    const tiny::Display& dsp = tiny::get_display();
    FrameExtents extents;
    unsigned long functions;
    unsigned long decorations;
    if (!Window::get_motif_hints(child, functions, decorations)) {
        extents = Client::default_frame_extents();
    } else if (decorations) {
        extents = Client::frame_extents_for(decorations);
    }
    const long values[] = {extents.left, extents.right, extents.top,
                           extents.bottom};
    XChangeProperty(display, child, dsp._NET_FRAME_EXTENTS, XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(values), 4);
}

bool Manager::on_root_message(const XClientMessageEvent& e)
{
    const tiny::Display& dsp = tiny::get_display();
    if (e.message_type == dsp._NET_REQUEST_FRAME_EXTENTS) {
        if (!wm_windows.count(e.window)) { // a managed window has them
            request_frame_extents(e.window);
        }
        return true;
    }
    if (e.message_type == dsp.WM_PROTOCOLS &&
        static_cast<Atom>(e.data.l[0]) == dsp._NET_WM_PING) {
        // the echo of a ping names the client in data[2] (EWMH)
        const auto it = wm_windows.find(static_cast<::Window>(e.data.l[2]));
        if (it != wm_windows.end()) {
            it->second->pong();
            if (asked == it->second) {
                close_unresponsive_dialog(); // it lives, no need to ask
            }
        }
        return true;
    }
    if (e.message_type == dsp._NET_CURRENT_DESKTOP ||
        e.message_type == dsp._NET_NUMBER_OF_DESKTOPS) {
        publish_desktops(); // there is one desktop, whatever is asked
        return true;
    }
    if (e.message_type == dsp._NET_WM_DESKTOP) {
        // only "all the desktops" or the one there is
        const auto found = wm_windows.find(e.window);
        const auto desktop =
                static_cast<unsigned long>(e.data.l[0]) & Window::all_desktops;
        if (found != wm_windows.end() &&
            (desktop == 0 || desktop == Window::all_desktops)) {
            found->second->set_sticky(desktop == Window::all_desktops);
        }
        return true;
    }
    if (e.message_type == dsp.WM_CHANGE_STATE) {
        // XIconifyWindow() of the client; it is only a request (ICCCM 4.1.4)
        const auto found = wm_windows.find(e.window);
        if (found != wm_windows.end() && e.data.l[0] == IconicState &&
            found->second->is_minimizable()) {
            found->second->minimize();
        }
        return true;
    }
    if (e.message_type != dsp._NET_ACTIVE_WINDOW &&
        e.message_type != dsp._NET_CLOSE_WINDOW) {
        return false; // the window knows the rest
    }
    const auto it = wm_windows.find(e.window);
    if (it == wm_windows.end()) {
        return true; // unknown window, nothing to do
    }
    if (e.message_type == dsp._NET_ACTIVE_WINDOW) {
        // data[0] is the source: 1 an application, 2 a pager (the user)
        const bool by_application = e.data.l[0] == 1;
        if (!by_application ||
            may_activate(it->second, static_cast<unsigned long>(e.data.l[1]))) {
            focus_window(it->second);
        } else {
            it->second->set_demands_attention(true); // the user decides
        }
    } else {
        it->second->close();
    }
    return true;
}

void Manager::catch_quit_signals()
{
    if (pipe(quit_pipe) != 0) {
        tiny::error("Can't create the pipe for signals, SIGTERM is not caught");
        quit_pipe[0] = quit_pipe[1] = -1;
        return;
    }
    for (const int fd : quit_pipe) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    }
    struct sigaction action = {};
    action.sa_handler = on_quit_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    for (size_t i = 0; i < sizeof(quit_signals) / sizeof(int); ++i) {
        sigaction(quit_signals[i], &action, &old_actions[i]);
    }
}

void Manager::release_quit_signals()
{
    for (size_t i = 0; i < sizeof(quit_signals) / sizeof(int); ++i) {
        sigaction(quit_signals[i], &old_actions[i], nullptr);
    }
    for (int& fd : quit_pipe) {
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }
}

int Manager::ping_timeout_ms() const
{
    const auto now = Window::Clock::now();
    int rv = -1;
    const auto soonest = [&](const Window::Clock::time_point at) {
        const auto left =
                std::chrono::ceil<std::chrono::milliseconds>(at - now);
        const int ms = static_cast<int>(std::max<long long>(left.count(), 0));
        rv = rv < 0 ? ms : std::min(rv, ms);
    };
    for (const auto& it : wm_windows) {
        if (const auto at = it.second->get_ping_wakeup()) {
            soonest(*at);
        }
        if (const auto at = it.second->get_ask_time(); at && !asked) {
            soonest(*at);
        }
    }
    return rv;
}

void Manager::set_power_commands(const std::string& restart,
                                 const std::string& shutdown)
{
    if (!restart.empty()) {
        restart_command = restart;
    }
    if (!shutdown.empty()) {
        shutdown_command = shutdown;
    }
}

int Manager::next_timeout_ms() const
{
    const int pings = ping_timeout_ms();
    const int others = timers.timeout_ms();
    if (pings < 0 || others < 0) {
        return std::max(pings, others);
    }
    return std::min(pings, others);
}

void Manager::check_pings()
{
    const auto now = Window::Clock::now();
    for (const auto& it : wm_windows) {
        it.second->expire_ping(now);
        it.second->ping_if_due(now);
    }
    ask_next();
}

void Manager::ask_next()
{
    if (asked) {
        return; // one question at a time, the others wait their turn
    }
    // The longest waiting first; each is checked again right before it is
    // shown, the situation may have changed since it was queued.
    const auto now = Window::Clock::now();
    for (;;) {
        Window* next = nullptr;
        Window::Clock::time_point next_at;
        for (const auto& it : wm_windows) {
            const auto at = it.second->get_ask_time();
            if (at && *at <= now && (!next || *at < next_at)) {
                next = it.second;
                next_at = *at;
            }
        }
        if (!next) {
            return;
        }
        if (next->client_process_gone()) {
            next->postpone_ask(now); // the connection is about to close
            continue;
        }
        ask_to_quit(next);
        return;
    }
}

void Manager::ask_to_quit(Window* window)
{
    asked = window;
    // the other windows of the process go away with it
    const pid_t pid = window->get_killable_pid();
    unsigned others = 0;
    for (const auto& it : wm_windows) {
        if (pid && it.second != window &&
            it.second->get_killable_pid() == pid) {
            ++others;
        }
    }
    wm_unresponsive.popup(window->get_wm_name(), others);
    wm_unresponsive.place_over(window->frame());
}

void Manager::close_unresponsive_dialog()
{
    asked = nullptr;
    wm_unresponsive.unmap();
}

bool Manager::wait_for_event()
{
    // A client that floods the queue must not delay the signal.
    for (bool wait = XPending(display) == 0;; wait = true) {
        struct pollfd fds[2] = {{ConnectionNumber(display), POLLIN, 0},
                                {quit_pipe[0], POLLIN, 0}};
        if (poll(fds, 2, wait ? next_timeout_ms() : 0) < 0 && errno != EINTR) {
            return false;
        }
        check_pings();
        timers.run_due();
        if (!running) {
            return false; // a timer asked to end the loop
        }
        if (fds[1].revents & POLLIN) {
            char drain[16];
            while (read(quit_pipe[0], drain, sizeof(drain)) > 0) {}
            return false;
        }
        if (XPending(display) > 0) {
            return true;
        }
    }
}

void Manager::main_loop()
{
    XEvent event;

    while (running) {
        reframe_windows();
        if (!wait_for_event()) {
            running = false; // SIGTERM, SIGHUP or SIGINT
            break;
        }
        XNextEvent(display, &event);
        const ::Window window = tiny::window_of_event(event);

        if (event.type == ClientMessage && on_root_message(event.xclient)) {
            continue;
        }

        if (dragged && event.type == MotionNotify) {
            while (XCheckTypedEvent(display, MotionNotify, &event)) {}
            move_resize_to(event.xmotion.x_root, event.xmotion.y_root);
            continue;
        }
        if (dragged && event.type == ButtonRelease) {
            // the other buttons and the wheel do not end it
            if (!drag_button || event.xbutton.button == drag_button) {
                stop_move_resize();
            }
            continue;
        }

        // a window raised (a full screen one, a client that asks for it) must
        // not hide the strip that shows the dock
        if ((event.type == ConfigureNotify || event.type == MapNotify) &&
            window != wm_dock_reveal.get_window() &&
            wm_dock_reveal.get_maped()) {
            XRaiseWindow(display, wm_dock_reveal.get_window());
        }

        auto signal_id = std::make_pair(event.type, window);

        // call manager handlers first
        if (tiny::handlers.count(signal_id)) {
            tiny::handlers.call_handler(signal_id, event);
            // the active window moved or got another size
            if (event.type == ConfigureNotify && active && !dragged &&
                window == active->frame()) {
                update_dock_visibility();
            }
            continue;
        }

        switch (event.type) {
            case KeyPress: // shortcuts
                on_key_press(event.xkey);
                break;
            case KeyRelease: // shortcuts
                on_key_release(event.xkey);
                break;
            case UnmapNotify: // need for delete windows frames (wm_windows)
                on_unmap_notify(event.xunmap);
                break;
            case SelectionClear: // another manager replaces us
                on_selection_clear(event.xselectionclear);
                break;
            case DestroyNotify: // the client is gone, also when minimized
                on_destroy_notify(event.xdestroywindow);
                break;
            case PropertyNotify: // the struts of a dock
                on_property_notify(event.xproperty);
                break;
            case MapRequest: // need to create windows frames for new windows
                on_map_request(event.xmaprequest);
                break;
            case ConfigureRequest: // need for resize requests from app windows
                on_configure_request(event.xconfigurerequest);
                break;
            case ConfigureNotify: // the screen has a new size
                on_root_configure(event.xconfigure);
                break;
            default:
#ifdef DEBUG
                fprintf(stderr, "Unhandled event: %s (%lx)\n",
                        tiny::event_to_string(event), window);
#endif
                break;
        }
    }
    reframe_windows();
}

void Manager::start_move_resize(Window* window, const XClientMessageEvent& e)
{
    const long direction = e.data.l[2];
    if (direction == MOVE_RESIZE_CANCEL) {
        if (dragged == window) {
            stop_move_resize();
        }
        return;
    }
    const unsigned edges = edges_of_direction(direction);
    // the keyboard variants are left out
    if (direction != MOVE_BY_POINTER && !edges) {
        return;
    }
    const bool allowed =
            edges ? window->is_resizable() && !window->get_maximized() &&
                            !window->is_fullscreen() && !window->is_shaded()
                  : window->is_movable() && !window->is_fullscreen();
    if (!allowed || !is_button_down()) {
        return;
    }
    stop_move_resize();

    const Cursor cursor = XCreateFontCursor(
            display, edges ? tiny::Position::get_cursor_shape(
                                     static_cast<uint16_t>(edges))
                           : XC_fleur);
    const int grabbed = XGrabPointer(
            display, root, False, ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
    XFreeCursor(display, cursor);
    if (grabbed != GrabSuccess) {
        return;
    }
    if (!is_button_down()) { // it was let go before the grab
        XUngrabPointer(display, CurrentTime);
        return;
    }
    dragged = window;
    drag_button = static_cast<unsigned>(e.data.l[3]);
    window->begin_drag(edges, static_cast<int>(e.data.l[0]),
                       static_cast<int>(e.data.l[1]));
}

void Manager::move_resize_to(int x_root, int y_root)
{
    if (dragged) {
        dragged->drag_to(x_root, y_root);
        update_dock_visibility();
    }
}

void Manager::stop_move_resize(bool finish)
{
    if (!dragged) {
        return;
    }
    Window* window = dragged;
    dragged = nullptr;
    XUngrabPointer(display, CurrentTime);
    if (finish) {
        window->end_drag();
        update_dock_visibility(); // not in the destructor, `active` may be gone
    }
}

void Manager::focus_window(Window* window)
{
    if (window->get_minimized()) {
        window->restore();
    }
    // the windows a dialog belongs to lie under it, the oldest at the bottom
    std::vector<Window*> parents;
    const Window* from = window;
    while (parents.size() < wm_tops.size()) {
        parents.push_back(parent_of(from));
        if (!parents.back()) {
            parents.pop_back();
            break;
        }
        from = parents.back();
    }
    for (auto it = parents.rbegin(); it != parents.rend(); ++it) {
        (*it)->raise();
    }
    // a modal dialog gets the focus instead of the window it belongs to
    Window* target = window;
    for (size_t depth = 0; depth < wm_tops.size(); ++depth) {
        target->raise();
        const std::vector<Window*> dialogs = modal_dialogs_of(target);
        if (dialogs.empty()) {
            break;
        }
        target = dialogs.back();
    }
    target->set_focus(); // raises the window as well
    for (Window* dialog : transients_of(target)) {
        dialog->raise(); // a dialog that is not modal stays over its parent
    }
    if (asked) {
        wm_unresponsive.place_over(asked->frame());
    }
    // do not wait for FocusIn, the next Alt + Tab can come before it
    remember_active(target, XEvent());
}

std::vector<Window*> Manager::switchable() const
{
    std::vector<Window*> found;
    std::copy_if(wm_tops.begin(), wm_tops.end(), std::back_inserter(found),
                 [&](const Window* window) {
                     return modal_dialogs_of(window).empty();
                 });
    if (found.empty()) { // dialogs of each other: leave nothing out of reach
        found.assign(wm_tops.begin(), wm_tops.end());
    }
    return found;
}

void Manager::activate_next_window()
{
    const std::vector<Window*> windows = switchable();
    if (windows.empty()) {
        return; // no window to activate
    }
    const auto it = std::find(windows.cbegin(), windows.cend(), active);
    focus_window(it == windows.cend() || it + 1 == windows.cend() ? windows[0]
                                                                  : *(it + 1));
}

void Manager::activate_prev_window()
{
    const std::vector<Window*> windows = switchable();
    if (windows.empty()) {
        return; // no window to activate
    }
    const auto it = std::find(windows.cbegin(), windows.cend(), active);
    focus_window(it == windows.cend() || it == windows.cbegin() ? windows.back()
                                                                : *(it - 1));
}

Window* Manager::previous_shown(const Window* window) const
{
    auto it = std::find(wm_tops.crbegin(), wm_tops.crend(), window);
    if (it == wm_tops.crend()) {
        return nullptr;
    }
    // walk back from the window, around the end of the list
    for (size_t i = 1; i < wm_tops.size(); ++i) {
        if (++it == wm_tops.crend()) {
            it = wm_tops.crbegin();
        }
        if (!(*it)->get_minimized() && parent_of(*it) != window) {
            return *it;
        }
    }
    return nullptr;
}

void Manager::set_application_icon(Window* window)
{
    if (window->get_wm_class().empty() &&
        window->get_wm_class_instance().empty()) {
        return;
    }
    const xdg::Application* app = wm_launcher.find_application(
            window->get_wm_class_instance(), window->get_wm_class());
    if (app == nullptr) {
        return;
    }
    window->set_app_id(app->id);
    const Launcher::Icon& icon = wm_launcher.application_icon(*app);
    if (icon.picture != None) {
        window->set_app_icon({icon.picture, icon.width, icon.height});
    }
}

void Manager::set_favorites(
        std::vector<std::string> ids,
        std::function<void(const std::vector<std::string>&)> changed)
{
    favorites = std::move(ids);
    favorites_changed = std::move(changed);
    update_favorites();
}

void Manager::update_favorites()
{
    std::vector<DockFavorite> shown;
    for (const std::string& id : favorites) {
        const xdg::Application* app = wm_launcher.find_application(id);
        if (app == nullptr) { // not installed (now), it stays in the list
            continue;
        }
        const Launcher::Icon& icon = wm_launcher.application_icon(*app);
        shown.push_back({id, {icon.picture, icon.width, icon.height}});
    }
    wm_dock.set_favorites(std::move(shown));
}

void Manager::toggle_favorite(const std::string& id)
{
    const auto it = std::find(favorites.begin(), favorites.end(), id);
    if (it == favorites.end()) {
        favorites.push_back(id);
    } else {
        favorites.erase(it);
    }
    update_favorites();
    if (favorites_changed) {
        favorites_changed(favorites);
    }
}

void Manager::move_favorite(const std::string& id, size_t place)
{
    // the favorites that are not installed are not in the dock, they keep their
    // places in the list
    std::vector<std::string> shown;
    for (const std::string& favorite : favorites) {
        if (wm_launcher.find_application(favorite) != nullptr) {
            shown.push_back(favorite);
        }
    }
    const auto it = std::find(shown.begin(), shown.end(), id);
    if (it == shown.end()) {
        return;
    }
    shown.erase(it);
    shown.insert(shown.begin() + static_cast<std::ptrdiff_t>(
                                         std::min(place, shown.size())),
                 id);
    auto next = shown.begin();
    for (std::string& favorite : favorites) {
        if (wm_launcher.find_application(favorite) != nullptr) {
            favorite = *next++;
        }
    }
    update_favorites();
    if (favorites_changed) {
        favorites_changed(favorites);
    }
}

void Manager::connect_window(Window* window)
{
    window->on_focus.connect(
            [this, window](const XEvent& e) { on_window_focus(window, e); });
    window->on_minimize.connect(
            [this, window](const XEvent&) { on_window_minimize(window); });
    window->on_unresponsive.connect([this](const XEvent&) { ask_next(); });
    window->on_modal.connect(
            [this, window](const XEvent&) { on_window_modal(window); });
    window->on_skip_taskbar.connect(
            [this, window](const XEvent&) { update_dock_entry(window); });
    window->on_layer.connect([this](const XEvent&) {
        restack_layers();
        publish_stacking();
    });
    window->on_attention.connect(
            [this, window](const XEvent& e) { wm_dock.redraw(window, e); });
    window->on_user_time.connect(
            [this, window](const XEvent&) { on_window_user_time(window); });
    window->on_reframe.connect(
            [this, window](const XEvent&) { on_window_reframe(window); });
    window->on_move_resize_request.connect([this, window](const XEvent& e) {
        start_move_resize(window, e.xclient);
    });
    window->on_modifier_drag.connect([this](const XEvent&) {
        super_down = false; // Super was used to move a window, not to open
    });
}

Window* Manager::parent_of(const Window* window) const
{
    const auto it = wm_windows.find(window->get_transient_for());
    return it == wm_windows.end() || it->second == window ? nullptr
                                                          : it->second;
}

bool Manager::is_dialog(const Window* window) const
{
    // a toolbar or a menu knows its place and has its own icon
    const Window::WMType type = window->get_wm_type();
    return parent_of(window) &&
           (type == Window::WMType::DIALOG || type == Window::WMType::NORMAL);
}

std::vector<Window*> Manager::transients_of(const Window* window) const
{
    std::vector<Window*> found;
    std::copy_if(wm_tops.begin(), wm_tops.end(), std::back_inserter(found),
                 [&](const Window* other) {
                     return other != window && is_dialog(other) &&
                            parent_of(other) == window;
                 });
    return found;
}

std::vector<Window*> Manager::modal_dialogs_of(const Window* window) const
{
    std::vector<Window*> found;
    const ::Window group = window->get_window_group();
    std::copy_if(wm_tops.begin(), wm_tops.end(), std::back_inserter(found),
                 [&](const Window* other) {
                     if (other == window || !other->is_modal()) {
                         return false;
                     }
                     if (parent_of(other)) { // modal for its parent only
                         return is_dialog(other) && parent_of(other) == window;
                     }
                     // no parent: modal for the whole group
                     return group != None &&
                            other->get_window_group() == group &&
                            !other->get_minimized();
                 });
    return found;
}

void Manager::center_over(Window* dialog, const Window* parent)
{
    XWindowAttributes outer;
    XWindowAttributes inner;
    if (!XGetWindowAttributes(display, parent->frame(), &outer) ||
        !XGetWindowAttributes(display, dialog->frame(), &inner)) {
        return;
    }
    const int x = std::min(outer.x + (outer.width - inner.width) / 2,
                           work_area.x + work_area.width - inner.width);
    const int y = std::min(outer.y + (outer.height - inner.height) / 2,
                           work_area.y + work_area.height - inner.height);
    dialog->move_to(std::max(x, work_area.x), std::max(y, work_area.y));
}

void Manager::on_window_focus(Window* window, const XEvent& e)
{
    if (modal_dialogs_of(window).empty()) {
        remember_active(window, e);
    } else {
        focus_window(window); // clicked on a window with a modal dialog
    }
}

void Manager::on_window_modal(const Window* dialog)
{
    if (!dialog->is_modal() || !active) {
        return;
    }
    const std::vector<Window*> blocking = modal_dialogs_of(active);
    if (std::find(blocking.begin(), blocking.end(), dialog) != blocking.end()) {
        focus_window(active); // the focus moves on to the dialog
    }
}

void Manager::update_dock_entry(Window* window)
{
    // a minimized window needs its icon, it is the way back
    const bool wanted = !is_dialog(window) &&
                        (!window->skip_taskbar() || window->get_minimized());
    if (wanted && !wm_dock.contains(window)) {
        wm_dock.add(window);
    } else if (!wanted && wm_dock.contains(window)) {
        wm_dock.remove(window);
    }
}

void Manager::on_window_reframe(const Window* window)
{
    // the window is in the middle of its own handler, so do it later
    const ::Window child = window->get_child();
    if (std::find(reframes.begin(), reframes.end(), child) == reframes.end()) {
        reframes.push_back(child);
    }
}

void Manager::reframe_windows()
{
    const std::vector<::Window> children = std::move(reframes);
    reframes.clear();
    for (const ::Window child : children) {
        reframe(child);
    }
}

void Manager::reframe(::Window child)
{
    const tiny::Display& dsp = tiny::get_display();
    const auto it = wm_windows.find(child);
    if (it == wm_windows.end()) {
        return;
    }
    const bool was_minimized = it->second->get_minimized();
    const bool was_active = it->second == active && !was_minimized;

    // withdrawing a window forgets its state, the client keeps it
    std::vector<Atom> states;
    Atom type;
    int format;
    unsigned long count = 0;
    unsigned long left;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display, child, dsp._NET_WM_STATE, 0, 32, False,
                           XA_ATOM, &type, &format, &count, &left,
                           &data) == Success &&
        data && format == 32) {
        const auto* atoms = reinterpret_cast<const Atom*>(data);
        states.assign(atoms, atoms + count);
    }
    if (data) {
        XFree(data);
    }

    // the reparenting unmaps the client, which is not its own wish
    reframed = {child, NextRequest(display), 0};
    unmanage(child, true, true);
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, child, &attrs)) {
        if (!states.empty()) {
            XChangeProperty(
                    display, child, dsp._NET_WM_STATE, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(states.data()),
                    static_cast<int>(states.size()));
        }
        Window* window = manage(child, attrs, false);
        XMapWindow(display, child);
        if (window && was_active) {
            focus_window(window);
        } else if (window && was_minimized) {
            window->minimize();
        }
    }
    XSync(display, False);
    reframed.last = NextRequest(display);
}

void Manager::remember_active(Window* window, const XEvent& e)
{
    if (active && active != window) {
        active->return_focus();
        wm_dock.redraw(active, e);
    }
    active = window;
    note_user_time(window); // focused by the user, or at least with a time
    wm_dock.redraw(active, e);
    restack_layers();
    publish_clients();
    update_dock_visibility();
}

void Manager::on_window_minimize(Window* window)
{
    if (asked == window && window->get_minimized()) {
        close_unresponsive_dialog(); // nobody looks at it, ask later
    }
    update_dock_entry(window);
    if (window == active) {
        publish_clients();        // _NET_ACTIVE_WINDOW
        update_dock_visibility(); // a minimized window covers nothing
    }
    // a dialog has no dock icon, so it goes and comes back with its parent
    Window* parent = parent_of(window);
    if (parent && parent->get_minimized() != window->get_minimized()) {
        if (window->get_minimized()) {
            parent->minimize();
        } else {
            parent->restore();
        }
    }
    for (Window* dialog : transients_of(window)) {
        if (dialog->get_minimized() == window->get_minimized()) {
            continue;
        }
        if (window->get_minimized()) {
            dialog->minimize();
        } else {
            dialog->restore();
        }
    }
}

void Manager::run_system_command(const std::string& command)
{
    if (command.empty()) {
        return;
    }
    if (tiny::spawn_command(command) < 0) {
        tiny::error("Can't run '%s': %s", command.c_str(), strerror(errno));
    }
}

void Manager::set_wallpaper_command(std::vector<std::string> command)
{
    wallpaper_command = std::move(command);
    run_program(wallpaper_command);
}

void Manager::set_dock_mode(DockMode mode)
{
    dock_mode = mode;
    dock_revealed = false;
    update_dock_visibility();
}

tiny::Rect Manager::dock_area() const
{
    return {screen_width / 2 - wm_dock.get_width() / 2,
            screen_height - wm_dock.get_height() - shell_theme.dock_border,
            wm_dock.get_width(), wm_dock.get_height()};
}

void Manager::update_dock_visibility()
{
    if (!wm_dock.get_realized()) {
        return;
    }
    const auto rect_of = [this](::Window window) {
        XWindowAttributes attrs;
        if (window == None || !XGetWindowAttributes(display, window, &attrs)) {
            return tiny::Rect{0, 0, 0, 0};
        }
        return tiny::Rect{attrs.x, attrs.y,
                          attrs.width + 2 * attrs.border_width,
                          attrs.height + 2 * attrs.border_width};
    };
    bool covered = false;
    if (dock_mode == DockMode::AutoHide && active && !active->get_minimized()) {
        // the dock is centered at the bottom (`Dock::resize`): no request
        const tiny::Rect dock_rect = dock_area();
        covered = overlaps(rect_of(active->frame()), dock_rect);
    }
    bool shown = dock_shown(dock_mode, covered, dock_revealed);
    // a menu of the dock, or an icon that is being dragged, keeps it
    const bool busy = wm_dock.get_menu().get_maped() ||
                      wm_dock.get_windows_menu().get_maped() ||
                      (dock_revealed && is_button_down && is_button_down());
    if (!shown && busy && wm_dock.get_maped()) {
        shown = true;
    }
    if (shown && !wm_dock.get_maped()) {
        wm_dock.map();
    } else if (!shown && wm_dock.get_maped()) {
        wm_dock.unmap();
    }
    // the strip is there only while the dock hides
    const bool strip = dock_mode == DockMode::AutoHide && !shown;
    if (strip) {
        // as wide as the dock: the pointer that reveals it is then over it
        // and leaves it, so it hides again
        const tiny::Rect area = dock_area();
        XMoveResizeWindow(display, wm_dock_reveal.get_window(), area.x,
                          screen_height - shell_theme.dock_reveal,
                          x_dim(area.width), x_dim(shell_theme.dock_reveal));
    }
    if (strip && !wm_dock_reveal.get_maped()) {
        wm_dock_reveal.map();
        XRaiseWindow(display, wm_dock_reveal.get_window());
    } else if (!strip && wm_dock_reveal.get_maped()) {
        wm_dock_reveal.unmap();
    }
}

void Manager::set_clock_format(const std::string& format)
{
    timers.cancel(clock_timer);
    clock_timer = 0;
    if (wm_clock) { // the panel must not keep a pointer to the deleted label
        wm_panel.remove(wm_clock.get());
        wm_clock.reset();
    }
    if (format.empty()) {
        return;
    }
    if (!wm_panel.get_realized()) {
        return;
    }
    wm_clock = std::make_unique<ClockLabel>(format);
    wm_panel.add_applet(wm_clock.get());
    wm_clock->set_events();
    tick_clock();
}

void Manager::tick_clock()
{
    wm_clock->update();
    clock_timer = timers.after(ClockLabel::until_next_minute(),
                               [this] { tick_clock(); });
}

void Manager::on_root_configure(const XConfigureEvent& e)
{
    if (e.window != root ||
        (e.width == screen_width && e.height == screen_height)) {
        return; // a window, or the root moved without a new size
    }
    screen_width = e.width;
    screen_height = e.height;
    update_dock_visibility();
    run_program(wallpaper_command); // nothing when there is no wallpaper
}

void Manager::run_program(const std::vector<std::string>& argv)
{
    if (argv.empty() || argv.front().empty()) {
        return;
    }
    // no shell and no splitting, the path of the program can have a blank
    if (tiny::spawn(argv) < 0) {
        tiny::error("Can't run '%s': %s", argv.front().c_str(),
                    strerror(errno));
    }
}

bool Manager::is_super(unsigned int keycode) const
{
    return keycode == XKeysymToKeycode(display, XK_Super_L) ||
           keycode == XKeysymToKeycode(display, XK_Super_R);
}

void Manager::on_key_press(const XKeyEvent& xkey)
{
    /* Alt F2 */
    if (xkey.keycode == XKeysymToKeycode(display, XK_F2)) {
        actions.run("run-dialog");
        key_done = true;
        return;
    }
    /* Super, the launcher opens when the key is released */
    if (is_super(xkey.keycode)) {
        super_down = true;
        key_done = true;
        return;
    }
    /* Alt + Tab or Alt + Shift + Tab; on press, because Alt can be released
     * before Tab and the release would not have the modifier any more */
    if (xkey.keycode == XKeysymToKeycode(display, XK_Tab) &&
        xkey.state & Mod1Mask) {
        actions.run(xkey.state & ShiftMask ? "prev-window" : "next-window");
        key_done = true;
        return;
    }
    key_done = false;
}

void Manager::on_key_release(const XKeyEvent& xkey)
{
    if (is_super(xkey.keycode)) {
        if (super_down) {
            actions.run("launcher");
        }
        super_down = false;
        return;
    }
    if (key_done) {
        return; // key is handled in on_key_press
    }

    fprintf(stderr, "%lu Manager::on_key_release Unhalded key event: %x\n",
            xkey.time, xkey.keycode);
}

Window* Manager::manage(::Window child, const XWindowAttributes& attrs,
                        bool fresh)
{
    std::set<Atom> properties;
    Window::get_properties(child, properties);

    if (properties.count(tiny::display->_NET_WM_WINDOW_TYPE)) {
        switch (Window::get_net_wm_type(child)) {
            case Window::WMType::DOCK:
                track_dock(child);
                return nullptr;
            case Window::WMType::DESKTOP:
                wm_desktops.insert(child);
                XLowerWindow(display, child);
                return nullptr; // it is not framed, but it stays at the bottom
            case Window::WMType::SPLASH:
                return nullptr; // do not manage this window type
            default:
                break;
        }
    }

    Window* window = nullptr;
    unsigned long functions;
    unsigned long decorations;
    if (Window::get_motif_hints(child, functions, decorations) &&
        !decorations) {
        window = Window::create(root, child, attrs, functions, work_area);
    } else {
        window = Client::create(root, child, attrs, functions, decorations,
                                work_area, fresh, &wm_icons);
    }

    // the client survives a crash of the manager
    XAddToSaveSet(display, child);

    wm_windows[child] = window;
    wm_tops.push_back(window);
    window->publish_desktop();
    window->publish_allowed_actions();
    window->apply_initial_state();
    restack_layers(); // it may be above or below from the start
    publish_clients();
    set_application_icon(window);
    if (is_dialog(window)) { // it has no place of its own in the dock
        center_over(window, parent_of(window));
        window->raise();
    } else if (!window->skip_taskbar()) {
        wm_dock.add(window);
    }
    connect_window(window);
    return window;
}

void Manager::unmanage(::Window child, bool alive, bool reframing)
{
    const auto it = wm_windows.find(child);
    if (it == wm_windows.end()) {
        return;
    }
    Window* window = it->second;
    if (asked == window) {
        close_unresponsive_dialog();
    }
    if (dragged == window) {
        stop_move_resize(false); // it is gone, or comes back in a new frame
    }

    Window* parent = parent_of(window);
    // the dialogs go with the window they belong to
    const std::vector<Window*> dialogs = transients_of(window);
    if (active == window && parent && !parent->get_minimized()) {
        wm_tops.remove(window);
        wm_windows.erase(it);
        active = nullptr;
        focus_window(parent); // back to the window the dialog belonged to
    } else if (active == window) {
        // a minimized window is not brought back by the closing of another
        Window* previous = previous_shown(window);
        if (previous) {
            focus_window(previous);
        }
    }
    if (active == window) {
        active = nullptr;
    }
    wm_tops.remove(window);
    wm_dock.remove(window);
    if (alive) {
        window->withdraw();
        XRemoveFromSaveSet(display, child);
    }
    delete window;
    wm_windows.erase(child);
    publish_clients();
    update_dock_visibility(); // a covering window is gone
    // an unmap comes before the destroy, so a withdrawn parent is the same
    for (Window* dialog : dialogs) {
        if (!reframing) {
            dialog->ask_to_close(); // never kill the client of a dialog
        }
    }
}

void Manager::release_windows()
{
    active = nullptr; // nothing is refocused on the way
    while (!wm_windows.empty()) {
        const ::Window child = wm_windows.begin()->first;
        Window* window = wm_windows.begin()->second;
        wm_tops.remove(window);
        wm_dock.remove(window);
        window->withdraw(); // the next manager starts from the client's word
        delete window;      // gives the client back to the root
        wm_windows.erase(child);
        XRemoveFromSaveSet(display, child);
        XMapWindow(display, child); // whoever comes next finds it shown
    }
    XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
    XSync(display, False);
}

void Manager::track_dock(::Window dock)
{
    XSelectInput(display, dock, PropertyChangeMask);
    wm_docks[dock] = read_struts(dock);
    update_work_area();
}

void Manager::untrack_dock(::Window dock)
{
    if (wm_docks.erase(dock)) {
        update_work_area();
    }
}

void Manager::publish_panel_strut()
{
    // like any panel: the others can see what is reserved
    const Struts strut = panel_strut();
    XWindowAttributes screen;
    XGetWindowAttributes(display, root, &screen);
    // top_start_x, top_end_x: the panel spans the whole width
    const long data[12] = {
            strut.left, strut.right, strut.top, strut.bottom,      0, 0,
            0,          0,           0,         screen.width - 1L, 0, 0};
    XChangeProperty(display, wm_panel.get_window(),
                    tiny::get_display()._NET_WM_STRUT_PARTIAL, XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(data), 12);
}

Struts Manager::panel_strut() const
{
    return {0, 0, wm_panel.get_height(), 0};
}

void Manager::update_work_area()
{
    XWindowAttributes screen;
    XGetWindowAttributes(display, root, &screen);
    std::vector<Struts> docks;
    docks.reserve(wm_docks.size() + 1);
    docks.push_back(panel_strut()); // our own panel is a dock like the others
    for (const auto& dock : wm_docks) {
        docks.push_back(dock.second);
    }
    const tiny::Rect area =
            compute_work_area(screen.width, screen.height, docks);
    if (area.x == work_area.x && area.y == work_area.y &&
        area.width == work_area.width && area.height == work_area.height) {
        return;
    }
    work_area = area;

    const long data[4] = {area.x, area.y, area.width, area.height};
    XChangeProperty(display, root, tiny::get_display()._NET_WORKAREA,
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(data), 4);
    for (const auto& window : wm_windows) {
        window.second->set_work_area(area);
    }
}

void Manager::on_property_notify(const XPropertyEvent& e)
{
    const tiny::Display& dsp = tiny::get_display();
    const auto dock = wm_docks.find(e.window);
    if (dock == wm_docks.end() ||
        (e.atom != dsp._NET_WM_STRUT && e.atom != dsp._NET_WM_STRUT_PARTIAL)) {
        return;
    }
    dock->second = read_struts(e.window);
    update_work_area();
}

void Manager::on_unmap_notify(const XUnmapEvent& e)
{
    untrack_dock(e.window); // a hidden dock reserves nothing
    wm_desktops.erase(e.window);
    // TODO: could be append to signal handlers on wm_window.window
    if (e.window == reframed.window && e.serial >= reframed.first &&
        e.serial < reframed.last) {
        return; // unmapped by the change of the frame
    }
    const auto it = wm_windows.find(e.window);
    if (it == wm_windows.end() || it->second->consume_unmap()) {
        return; // unknown, or hidden by the manager itself
    }
    unmanage(e.window, true);
}

void Manager::on_selection_clear(const XSelectionClearEvent& e)
{
    if (selection && selection->is_lost_by(e)) {
        tiny::error("Another window manager takes over, leaving");
        running = false;
    }
}

void Manager::on_destroy_notify(const XDestroyWindowEvent& e)
{
    untrack_dock(e.window);
    wm_desktops.erase(e.window);
    unmanage(e.window, false);
}

void Manager::on_map_request(const XMapRequestEvent& e)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, e.window, &attrs);
    if (attrs.map_state == IsViewable) {
        return;
    }
    if (attrs.override_redirect) {
        return; // menu, special tools (idesk) and so on
    }

    const auto known = wm_windows.find(e.window);
    if (known != wm_windows.end()) { // a minimized client shows itself
        focus_window(known->second);
        return;
    }

    Window* window = manage(e.window, attrs);
    XMapWindow(display, e.window);
    if (!window) { // the type of the window is not managed
        return;
    }
    if (may_take_focus(window)) {
        window->set_focus();
    } else {
        window->set_demands_attention(true);  // the user decides
        window->stack_under(active->frame()); // it does not cover the user
    }
}

bool Manager::may_take_focus(const Window* window) const
{
    if (!active || active->get_minimized() || parent_of(window) == active) {
        return true; // nothing to steal, or a dialog of the window in use
    }
    unsigned long time = 0;
    const bool has_time = Window::get_user_time(window->get_child(), time);
    return takes_focus(has_time, time, has_user_time, last_user_time);
}

bool Manager::may_activate(const Window* window, unsigned long time) const
{
    if (!active || active->get_minimized() || window == active ||
        parent_of(window) == active) {
        return true; // nothing to steal
    }
    // an application has to tell the time of the action of the user
    return takes_focus(true, time, has_user_time, last_user_time);
}

void Manager::on_window_user_time(const Window* window)
{
    if (window != active) {
        return; // only the window in use tells the time of the user
    }
    note_user_time(window);
}

void Manager::note_user_time(const Window* window)
{
    unsigned long time = 0;
    if (Window::get_user_time(window->get_child(), time) && time != 0 &&
        takes_focus(true, time, has_user_time, last_user_time)) {
        last_user_time = time;
        has_user_time = true;
    }
}

void Manager::on_configure_request(const XConfigureRequestEvent& e)
{
    const auto it = wm_windows.find(e.window);
    if (it != wm_windows.end()) {
        it->second->configure_request(e); // the frame moves with the client
        return;
    }

    XWindowChanges ch;
    ch.x = e.x;
    ch.y = e.y;
    ch.width = e.width;
    ch.height = e.height;
    ch.border_width = e.border_width;
    ch.sibling = e.above;
    ch.stack_mode = e.detail;

    XConfigureWindow(display, e.window, static_cast<unsigned int>(e.value_mask),
                     &ch);
}

int Manager::on_error(::Display* display, XErrorEvent* error)
{
    // A client can be gone while the manager reads its properties or moves
    // it: that is a race, not a fault.
    if (error->error_code == BadWindow || error->error_code == BadDrawable) {
        return 0;
    }
    char buffer[255];
    XGetErrorText(display, error->error_code, buffer, 255);
    tiny::error("XError %s (%d) opcode %d/%d on res %lx", buffer,
                error->error_code, error->request_code, error->minor_code,
                error->resourceid);
    return 0;
}

} // namespace tiny::wm
