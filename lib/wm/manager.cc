#include <algorithm>
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

#include "object.h"
#include "spawn.h"
#include "wm/manager.h"
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
    XSetWindowBackground(display_arg, root_arg, tiny::theme.root_background);
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

    wm_panel.realize(root_arg, 0, 0);
    wm_panel.set_events();

    wm_dock.realize(root_arg, attrs.width / 2,
                    attrs.height - wm_dock.get_height() -
                            tiny::theme.wm_dock_border);
    wm_dock.set_events();

    wm_run.realize(root_arg, 0, 0);
    wm_run.set_events();

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

            manage(rv_child[i], child_attrs);
        }
        XFree(rv_child);
    }

    if (!wm_tops.empty()) {
        wm_tops.back()->set_focus(); // the topmost one
    }

    set_supported();
    publish_clients();

    wm_panel.map_all();
    wm_dock.map_all();

    wm_dock.get_menu_button().on_click.connect(
            this, static_cast<tiny::object_signal_t>(&Manager::on_launcher));
    wm_panel.menu_user.restart.on_click.connect(
            this, static_cast<tiny::object_signal_t>(&Manager::on_restart));
    wm_panel.menu_user.shutdown.on_click.connect(
            this, static_cast<tiny::object_signal_t>(&Manager::on_shutdown));
    wm_panel.menu_user.logout.on_click.connect(
            this, static_cast<tiny::object_signal_t>(&Manager::on_logout));

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
    clear_published();
    release_quit_signals();

    if (old_error_handler) {
        XSetErrorHandler(old_error_handler);
    }
    // the display belongs to the caller, only stop managing the root window
    XSelectInput(display, root, 0);
}

void Manager::set_events()
{
    XSelectInput(display, root,
                 SubstructureNotifyMask | SubstructureRedirectMask);

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

void Manager::clear_published()
{
    const tiny::Display& dsp = tiny::get_display();
    for (const Atom property :
         {dsp._NET_SUPPORTED, dsp._NET_SUPPORTING_WM_CHECK,
          dsp._NET_CLIENT_LIST, dsp._NET_CLIENT_LIST_STACKING,
          dsp._NET_ACTIVE_WINDOW}) {
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
    // below first, so the one that is above comes out on top of all
    for (Window* window : wm_tops) {
        if (window->is_below() && !window->get_minimized()) {
            window->lower();
        }
    }
    for (const ::Window desktop : wm_desktops) { // under all the others
        XLowerWindow(display, desktop);
    }
    // the focused window last, so it stays on top of the others in its layer
    const auto raise_above = [this](Window* window) {
        if (window->is_above() && !window->get_minimized()) {
            window->raise();
            for (Window* dialog : transients_of(window)) {
                dialog->raise(); // a dialog stays over its parent
            }
        }
    };
    for (Window* window : wm_tops) {
        if (window != active) {
            raise_above(window);
        }
    }
    if (active) {
        raise_above(active);
    }
}

void Manager::request_frame_extents(::Window child)
{
    const tiny::Display& dsp = tiny::get_display();
    FrameExtents extents;
    unsigned long functions;
    unsigned long decorations;
    if (!Window::get_motif_hints(child, functions, decorations) ||
        decorations) {
        extents = Client::default_frame_extents();
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
    if (e.message_type != dsp._NET_ACTIVE_WINDOW &&
        e.message_type != dsp._NET_CLOSE_WINDOW) {
        return false; // the window knows the rest
    }
    const auto it = wm_windows.find(e.window);
    if (it == wm_windows.end()) {
        return true; // unknown window, nothing to do
    }
    if (e.message_type == dsp._NET_ACTIVE_WINDOW) {
        focus_window(it->second);
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

bool Manager::wait_for_event()
{
    // A client that floods the queue must not delay the signal.
    for (bool wait = XPending(display) == 0;; wait = true) {
        struct pollfd fds[2] = {{ConnectionNumber(display), POLLIN, 0},
                                {quit_pipe[0], POLLIN, 0}};
        if (poll(fds, 2, wait ? -1 : 0) < 0 && errno != EINTR) {
            return false;
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
        if (!wait_for_event()) {
            running = false; // SIGTERM, SIGHUP or SIGINT
            break;
        }
        XNextEvent(display, &event);
        const ::Window window = tiny::window_of_event(event);

        if (event.type == ClientMessage && on_root_message(event.xclient)) {
            continue;
        }

        auto signal_id = std::make_pair(event.type, window);

        // call manager handlers first
        if (tiny::handlers.count(signal_id)) {
            tiny::handlers.call_hanlder(signal_id, event);
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
            default:
#ifdef DEBUG
                fprintf(stderr, "Unhandled event: %s (%lx)\n",
                        tiny::event_to_string(event), window);
#endif
                break;
        }
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
    const Launcher::Icon& icon = wm_launcher.application_icon(
            window->get_wm_class_instance(), window->get_wm_class());
    if (icon.picture != None) {
        window->set_app_icon({icon.picture, icon.width, icon.height});
    }
}

void Manager::connect_window(Window* window)
{
    window->on_focus.connect(this, static_cast<tiny::object_signal_t>(
                                           &Manager::on_window_focus));
    window->on_minimize.connect(this, static_cast<tiny::object_signal_t>(
                                              &Manager::on_window_minimize));
    window->on_modal.connect(this, static_cast<tiny::object_signal_t>(
                                           &Manager::on_window_modal));
    window->on_skip_taskbar.connect(this,
                                    static_cast<tiny::object_signal_t>(
                                            &Manager::on_window_skip_taskbar));
    window->on_layer.connect(this, static_cast<tiny::object_signal_t>(
                                           &Manager::on_window_layer));
    window->on_attention.connect(this, static_cast<tiny::object_signal_t>(
                                               &Manager::on_window_attention));
    window->on_user_time.connect(this, static_cast<tiny::object_signal_t>(
                                               &Manager::on_window_user_time));
    window->on_modifier_drag.connect(
            this, static_cast<tiny::object_signal_t>(&Manager::on_window_drag));
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

void Manager::on_window_drag(tiny::Object*, const XEvent&, void*)
{
    super_down = false; // Super was used to move a window, not to open
}

void Manager::on_window_focus(tiny::Object* o, const XEvent& e, void*)
{
    auto* window = reinterpret_cast<Window*>(o);
    if (modal_dialogs_of(window).empty()) {
        remember_active(window, e);
    } else {
        focus_window(window); // clicked on a window with a modal dialog
    }
}

void Manager::on_window_modal(tiny::Object* o, const XEvent&, void*)
{
    const auto* dialog = reinterpret_cast<Window*>(o);
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

void Manager::on_window_skip_taskbar(tiny::Object* o, const XEvent&, void*)
{
    update_dock_entry(reinterpret_cast<Window*>(o));
}

void Manager::on_window_layer(tiny::Object*, const XEvent&, void*)
{
    restack_layers();
    publish_stacking();
}

void Manager::on_window_attention(tiny::Object* o, const XEvent& e, void*)
{
    wm_dock.redraw(reinterpret_cast<Window*>(o), e);
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
}

void Manager::on_window_minimize(tiny::Object* o, const XEvent&, void*)
{
    auto* window = reinterpret_cast<Window*>(o);
    update_dock_entry(window);
    if (window == active) {
        publish_clients(); // _NET_ACTIVE_WINDOW
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

void Manager::on_logout(tiny::Object*, const XEvent&, void*)
{
    running = false;
}

void Manager::on_restart(tiny::Object*, const XEvent&, void*)
{
    run_system_command(restart_command);
}

void Manager::on_shutdown(tiny::Object*, const XEvent&, void*)
{
    run_system_command(shutdown_command);
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

void Manager::on_launcher(tiny::Object*, const XEvent&, void*)
{
    wm_launcher.toggle();
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
        wm_run.popup();
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
        if (xkey.state & ShiftMask) {
            activate_prev_window();
        } else {
            activate_next_window();
        }
        key_done = true;
        return;
    }
    key_done = false;
}

void Manager::on_key_release(const XKeyEvent& xkey)
{
    if (is_super(xkey.keycode)) {
        if (super_down) {
            wm_launcher.toggle();
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

Window* Manager::manage(::Window child, const XWindowAttributes& attrs)
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
                                work_area);
    }

    // the client survives a crash of the manager
    XAddToSaveSet(display, child);

    wm_windows[child] = window;
    wm_tops.push_back(window);
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

void Manager::unmanage(::Window child, bool alive)
{
    const auto it = wm_windows.find(child);
    if (it == wm_windows.end()) {
        return;
    }
    Window* window = it->second;

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
    // an unmap comes before the destroy, so a withdrawn parent is the same
    for (Window* dialog : dialogs) {
        dialog->ask_to_close(); // never kill the client of a dialog
    }
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

void Manager::update_work_area()
{
    XWindowAttributes screen;
    XGetWindowAttributes(display, root, &screen);
    std::vector<Struts> docks;
    docks.reserve(wm_docks.size());
    for (const auto& dock : wm_docks) {
        docks.push_back(dock.second);
    }
    const tiny::Rect area = compute_work_area(screen.width, screen.height,
                                              tiny::theme.wm_panel, docks);
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
    const auto it = wm_windows.find(e.window);
    if (it == wm_windows.end() || it->second->get_minimized()) {
        return; // unknown, or hidden by the manager itself
    }
    unmanage(e.window, true);
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
        window->set_demands_attention(true); // the user decides
    }
}

bool Manager::may_take_focus(Window* window) const
{
    if (!active || active->get_minimized() || parent_of(window) == active) {
        return true; // nothing to steal, or a dialog of the window in use
    }
    unsigned long time = 0;
    const bool has_time = Window::get_user_time(window->get_child(), time);
    return takes_focus(has_time, time, has_user_time, last_user_time);
}

void Manager::on_window_user_time(tiny::Object* o, const XEvent&, void*)
{
    auto* window = reinterpret_cast<Window*>(o);
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
