#include "fake_client.h"
#include "helpers.h"

#include <X11/keysym.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <csignal>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "wm/dock.h"
#include "wm/panel.h"
#include "wm/run_dialog.h"
#include "wm/window.h"
#include "wm/client.h"

// The tests look at (and prepare) the state the manager keeps private.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "wm/manager.h"
#undef private
#ifdef __clang__
#pragma clang diagnostic pop
#endif

using WmWindow = tiny::wm::Window;

namespace {

int border()
{
    return tiny::theme.wm_dock_border;
}

// Stops the main loop: the loop reaches it after all earlier events.
class Stopper : public tiny::Object {
  public:
    Stopper(bool* running_arg): running(running_arg) {}

    void on_event(const XEvent&, void*) { *running = false; }

    tiny::event_signal_t slot()
    {
        return static_cast<tiny::event_signal_t>(&Stopper::on_event);
    }

  private:
    bool* running;
};

class ManagerTest : public ::testing::Test {
  protected:
    ~ManagerTest() override
    {
        if (manager) {
            for (auto& it : manager->wm_windows) {
                delete it.second;
            }
            manager->wm_windows.clear();
            manager->wm_tops.clear();
            manager.reset();
        }
    }

    // A client window of the test (10,20 200x100); it is created at once and
    // mapped when `mapped` is set (mapping needs the manager when it runs).
    FakeClient& add_client(bool mapped = true)
    {
        clients.push_back(std::make_unique<FakeClient>());
        if (mapped) {
            clients.back()->map();
        }
        return *clients.back();
    }

    void start()
    {
        ::Display* display = tiny::get_display();
        manager = std::make_unique<tiny::wm::Manager>(display, root_window());
        XSync(display, False); // the server is grabbed until the ungrab
    }

    // Runs the main loop until it handled every event sent so far.
    void run_loop()
    {
        const ::Window sentinel =
                XCreateSimpleWindow(dsp(), root_window(), 0, 0, 1, 1, 0, 0, 0);
        Stopper stopper(&manager->running);
        tiny::handlers.set_handler(CreateNotify, sentinel, &stopper,
                                   stopper.slot(), nullptr);
        XSync(dsp(), False);
        manager->main_loop();
        tiny::handlers.unset_handler(CreateNotify, sentinel);
        XDestroyWindow(dsp(), sentinel);
        manager->running = true;
    }

    ::Window parent_of(::Window w)
    {
        XSync(dsp(), False);
        ::Window root;
        ::Window parent;
        ::Window* kids = nullptr;
        unsigned int count;
        XQueryTree(dsp(), w, &root, &parent, &kids, &count);
        if (kids) {
            XFree(kids);
        }
        return parent;
    }

    bool is_managed(const FakeClient& c) const
    {
        return manager->wm_windows.count(c.window) == 1;
    }

    // `window` is wrapped in a frame
    bool is_framed(const FakeClient& c)
    {
        return parent_of(c.window) != root_window();
    }

    void set_window_type(FakeClient& c, Atom type)
    {
        c.set_atoms(dsp()._NET_WM_WINDOW_TYPE, {type});
    }

    XKeyEvent key(KeySym keysym, unsigned int state = Mod1Mask)
    {
        XKeyEvent e = {};
        e.type = KeyPress;
        e.keycode = XKeysymToKeycode(dsp(), keysym);
        e.state = state;
        return e;
    }

    // Three managed windows, `active` is the first one.
    void manage_three()
    {
        for (int i = 0; i < 3; ++i) {
            add_client();
        }
        start();
        ASSERT_EQ(manager->wm_tops.size(), 3u);
        manager->active = manager->wm_tops.front();
    }

    HeaderFont header_font;
    WidgetFont widget_font;
    std::vector<std::unique_ptr<FakeClient>> clients;
    std::unique_ptr<tiny::wm::Manager> manager;
};

// ---- construction ----

TEST_F(ManagerTest, shows_the_panel_the_dock_and_keeps_the_run_dialog_hidden)
{
    start();

    EXPECT_EQ(attributes_of(manager->wm_panel).map_state, IsViewable);
    EXPECT_EQ(attributes_of(manager->wm_dock).map_state, IsViewable);
    EXPECT_EQ(attributes_of(manager->wm_run).map_state, IsUnmapped);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsUnmapped);
}

TEST_F(ManagerTest, dock_is_centered_at_the_bottom)
{
    start();

    auto root = attributes_of(root_window());
    auto dock = attributes_of(manager->wm_dock);
    EXPECT_EQ(dock.x, root.width / 2);
    EXPECT_EQ(dock.y, root.height - manager->wm_dock.get_height() - border());
}

TEST_F(ManagerTest, connects_logout_of_the_user_menu)
{
    start();
    ASSERT_TRUE(manager->running);

    auto& logout = manager->wm_panel.menu_user.logout;
    logout.on_click(&logout, make_event(ButtonRelease));

    EXPECT_FALSE(manager->running);
}

namespace {

template <class Done>
bool wait_for(Done done)
{
    for (int i = 0; i < 300; ++i) {
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::filesystem::path marker_file(const char* name)
{
    auto path = std::filesystem::temp_directory_path() /
                (std::string(name) + std::to_string(getpid()));
    std::filesystem::remove(path);
    return path;
}

} // namespace

TEST_F(ManagerTest, restart_of_the_user_menu_runs_its_command)
{
    start();
    const auto marker = marker_file("tiny-manager-restart-");
    manager->restart_command = "touch " + marker.string();

    auto& restart = manager->wm_panel.menu_user.restart;
    restart.on_click(&restart, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    EXPECT_TRUE(manager->running); // only the command is started
    std::filesystem::remove(marker);
}

TEST_F(ManagerTest, shutdown_of_the_user_menu_runs_its_command)
{
    start();
    const auto marker = marker_file("tiny-manager-shutdown-");
    manager->shutdown_command = "touch " + marker.string();

    auto& shutdown = manager->wm_panel.menu_user.shutdown;
    shutdown.on_click(&shutdown, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
}

TEST_F(ManagerTest, an_empty_command_of_the_user_menu_does_nothing)
{
    start();
    manager->shutdown_command.clear();

    auto& shutdown = manager->wm_panel.menu_user.shutdown;
    shutdown.on_click(&shutdown, make_event(ButtonRelease));

    EXPECT_TRUE(manager->running);
}

TEST_F(ManagerTest, a_missing_command_of_the_user_menu_is_reported)
{
    start();
    manager->restart_command = "tiny-no-such-program";

    ::testing::internal::CaptureStderr();
    auto& restart = manager->wm_panel.menu_user.restart;
    restart.on_click(&restart, make_event(ButtonRelease));

    EXPECT_NE(::testing::internal::GetCapturedStderr().find(
                      "tiny-no-such-program"),
              std::string::npos);
}

TEST_F(ManagerTest, signals_end_the_main_loop)
{
    for (const int number : {SIGTERM, SIGHUP, SIGINT}) {
        start();
        ASSERT_TRUE(manager->running);

        raise(number);
        manager->main_loop();

        EXPECT_FALSE(manager->running) << "signal " << number;
        manager.reset();
    }
}

TEST_F(ManagerTest, adopts_the_mapped_windows)
{
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();

    start();

    EXPECT_EQ(manager->wm_windows.size(), 2u);
    EXPECT_EQ(manager->wm_tops.size(), 2u);
    EXPECT_TRUE(is_framed(first));
    EXPECT_TRUE(is_framed(second));
}

TEST_F(ManagerTest, only_the_last_adopted_window_gets_the_focus)
{
    const FakeClient& first = add_client();
    const FakeClient& last = add_client();

    start();

    EXPECT_FALSE(manager->wm_windows[first.window]->is_focused());
    EXPECT_TRUE(manager->wm_windows[last.window]->is_focused());
}

TEST_F(ManagerTest, adopted_windows_get_an_icon_in_the_dock)
{
    add_client();
    add_client();
    pump_events();
    start();
    pump_events();

    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), 3u);
}

TEST_F(ManagerTest, windows_that_skip_the_taskbar_have_no_dock_icon)
{
    FakeClient& c = add_client();
    c.set_state({dsp()._NET_WM_STATE_SKIP_TASKBAR});

    start();
    pump_events();

    EXPECT_TRUE(is_managed(c));
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), 1u);
}

TEST_F(ManagerTest, unmapped_windows_are_not_adopted)
{
    const FakeClient& c = add_client(false);

    start();

    EXPECT_FALSE(is_managed(c));
    EXPECT_FALSE(is_framed(c));
}

TEST_F(ManagerTest, override_redirect_windows_are_not_adopted)
{
    FakeClient& c = add_client(false);
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    XChangeWindowAttributes(c.display, c.window, CWOverrideRedirect, &attrs);
    c.map();

    start();

    EXPECT_FALSE(is_managed(c));
}

TEST_F(ManagerTest, desktop_dock_and_splash_windows_are_not_adopted)
{
    for (const Atom type :
         {dsp()._NET_WM_WINDOW_TYPE_DESKTOP, dsp()._NET_WM_WINDOW_TYPE_DOCK,
          dsp()._NET_WM_WINDOW_TYPE_SPLASH}) {
        FakeClient& c = add_client(false);
        set_window_type(c, type);
        c.map();
    }

    start();

    EXPECT_EQ(manager->wm_windows.size(), 0u);
}

TEST_F(ManagerTest, other_window_types_are_adopted)
{
    FakeClient& c = add_client(false);
    set_window_type(c, dsp()._NET_WM_WINDOW_TYPE_DIALOG);
    c.map();

    start();

    EXPECT_TRUE(is_managed(c));
}

TEST_F(ManagerTest, window_without_decorations_is_not_framed)
{
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.map();

    start();

    ASSERT_TRUE(is_managed(c));
    EXPECT_FALSE(is_framed(c));
    EXPECT_EQ(dynamic_cast<tiny::wm::Client*>(manager->wm_windows[c.window]),
              nullptr);
}

TEST_F(ManagerTest, window_with_decorations_is_framed)
{
    const FakeClient& c = add_client();

    start();

    ASSERT_TRUE(is_managed(c));
    EXPECT_NE(dynamic_cast<tiny::wm::Client*>(manager->wm_windows[c.window]),
              nullptr);
}

TEST_F(ManagerTest, grabs_the_shortcuts_of_the_manager)
{
    start();

    const XErrors errors;
    const FakeClient other;
    for (const int sym : {XK_Tab, XK_F2}) {
        XGrabKey(other.display,
                 XKeysymToKeycode(other.display, static_cast<KeySym>(sym)),
                 Mod1Mask, root_window(), True, GrabModeAsync, GrabModeAsync);
    }
    other.sync();

    EXPECT_EQ(errors.errors(), 2); // BadAccess: already grabbed
}

TEST_F(ManagerTest, grabs_the_super_keys_for_the_launcher)
{
    start();

    const XErrors errors;
    const FakeClient other;
    for (const int sym : {XK_Super_L, XK_Super_R}) {
        XGrabKey(other.display,
                 XKeysymToKeycode(other.display, static_cast<KeySym>(sym)), 0,
                 root_window(), True, GrabModeAsync, GrabModeAsync);
    }
    other.sync();

    EXPECT_EQ(errors.errors(), 2); // BadAccess: already grabbed
}

// ---- EWMH ----

namespace {

// The values of a property of the root window.
template <typename T>
std::vector<T> root_property(Atom property, Atom type)
{
    XSync(dsp(), False);
    Atom actual;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    std::vector<T> values;
    if (XGetWindowProperty(dsp(), root_window(), property, 0, 1024, False, type,
                           &actual, &format, &count, &left, &data) == Success &&
        data) {
        const auto* first = reinterpret_cast<const T*>(data);
        values.assign(first, first + count);
        XFree(data);
    }
    return values;
}

// A request of a client to the window manager, sent to the root window.
void request(::Window window, Atom message, long data0 = 0)
{
    XEvent e = {};
    e.xclient.type = ClientMessage;
    e.xclient.window = window;
    e.xclient.message_type = message;
    e.xclient.format = 32;
    e.xclient.data.l[0] = data0;
    XSendEvent(dsp(), root_window(), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XSync(dsp(), False);
}

} // namespace

TEST_F(ManagerTest, the_supported_properties_are_published_on_the_root)
{
    start();

    const auto atoms = root_property<Atom>(dsp()._NET_SUPPORTED, XA_ATOM);

    EXPECT_NE(std::count(atoms.begin(), atoms.end(), dsp()._NET_ACTIVE_WINDOW),
              0);
    EXPECT_NE(std::count(atoms.begin(), atoms.end(), dsp()._NET_CLIENT_LIST),
              0);
    EXPECT_NE(std::count(atoms.begin(), atoms.end(), dsp()._NET_CLOSE_WINDOW),
              0);
    EXPECT_NE(std::count(atoms.begin(), atoms.end(), dsp()._NET_WM_STATE), 0);
}

TEST_F(ManagerTest, a_check_window_with_the_name_proves_the_manager_runs)
{
    start();

    const auto on_root =
            root_property<::Window>(dsp()._NET_SUPPORTING_WM_CHECK, XA_WINDOW);
    ASSERT_EQ(on_root.size(), 1u);
    XSync(dsp(), False);
    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dsp(), on_root[0], dsp()._NET_WM_NAME, 0, 64,
                                 False, dsp().UTF8_STRING, &type, &format,
                                 &count, &left, &data),
              Success);
    ASSERT_NE(data, nullptr);
    EXPECT_STREQ(reinterpret_cast<const char*>(data), "tiny-shell");
    XFree(data);

    // the window points to itself
    ASSERT_EQ(XGetWindowProperty(
                      dsp(), on_root[0], dsp()._NET_SUPPORTING_WM_CHECK, 0, 1,
                      False, XA_WINDOW, &type, &format, &count, &left, &data),
              Success);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(*reinterpret_cast<::Window*>(data), on_root[0]);
    XFree(data);
}

TEST_F(ManagerTest, the_client_list_follows_the_managed_windows)
{
    start();
    EXPECT_TRUE(
            root_property<::Window>(dsp()._NET_CLIENT_LIST, XA_WINDOW).empty());
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();

    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST, XA_WINDOW)),
              (std::vector<::Window>{first.window, second.window}));

    XUnmapWindow(first.display, first.window);
    first.sync();
    run_loop();
    pump_events();

    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST, XA_WINDOW)),
              (std::vector<::Window>{second.window}));
}

TEST_F(ManagerTest, the_active_window_follows_the_focus)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();

    EXPECT_EQ((root_property<::Window>(dsp()._NET_ACTIVE_WINDOW, XA_WINDOW)),
              (std::vector<::Window>{c.window}));
}

TEST_F(ManagerTest, no_active_window_is_published_when_the_last_one_is_gone)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();

    XUnmapWindow(c.display, c.window);
    c.sync();
    run_loop();
    pump_events();

    EXPECT_EQ((root_property<::Window>(dsp()._NET_ACTIVE_WINDOW, XA_WINDOW)),
              (std::vector<::Window>{None}));
}

TEST_F(ManagerTest, active_window_request_focuses_the_window)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();
    ASSERT_EQ(manager->active, manager->wm_windows[second.window]);

    request(first.window, dsp()._NET_ACTIVE_WINDOW);
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);
    EXPECT_EQ((root_property<::Window>(dsp()._NET_ACTIVE_WINDOW, XA_WINDOW)),
              (std::vector<::Window>{first.window}));
}

TEST_F(ManagerTest, active_window_request_restores_a_minimized_window)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    manager->wm_windows[c.window]->minimize();
    ASSERT_TRUE(manager->wm_windows[c.window]->get_minimized());

    request(c.window, dsp()._NET_ACTIVE_WINDOW);
    run_loop();
    pump_events();

    EXPECT_FALSE(manager->wm_windows[c.window]->get_minimized());
}

TEST_F(ManagerTest, close_window_request_asks_the_client_to_close)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp().WM_DELETE_WINDOW});
    XSelectInput(c.display, c.window, NoEventMask);
    run_loop();
    pump_events();

    request(c.window, dsp()._NET_CLOSE_WINDOW);
    run_loop();

    XEvent e;
    c.sync(); // the manager sends the message over its own connection
    ASSERT_TRUE(XCheckTypedEvent(c.display, ClientMessage, &e));
    EXPECT_EQ(static_cast<Atom>(e.xclient.data.l[0]), dsp().WM_DELETE_WINDOW);
}

TEST_F(ManagerTest, requests_of_an_unknown_window_are_ignored)
{
    start();
    const FakeClient& c = add_client(false); // never mapped, never managed

    request(c.window, dsp()._NET_ACTIVE_WINDOW);
    request(c.window, dsp()._NET_CLOSE_WINDOW);
    run_loop();

    EXPECT_TRUE(manager->wm_windows.empty());
}

TEST_F(ManagerTest, destructor_removes_what_it_published)
{
    start();
    const auto check =
            root_property<::Window>(dsp()._NET_SUPPORTING_WM_CHECK, XA_WINDOW);
    ASSERT_EQ(check.size(), 1u);

    manager.reset();

    EXPECT_TRUE(root_property<Atom>(dsp()._NET_SUPPORTED, XA_ATOM).empty());
    EXPECT_TRUE(
            root_property<::Window>(dsp()._NET_SUPPORTING_WM_CHECK, XA_WINDOW)
                    .empty());
    EXPECT_TRUE(
            root_property<::Window>(dsp()._NET_CLIENT_LIST, XA_WINDOW).empty());
    XWindowAttributes attrs;
    const XErrors errors;
    EXPECT_EQ(XGetWindowAttributes(dsp(), check[0], &attrs), 0); // destroyed
}

// ---- destruction ----

TEST_F(ManagerTest, destructor_releases_the_shortcuts_and_the_root_window)
{
    start();
    manager.reset();

    const XErrors errors;
    const FakeClient other;
    XGrabKey(other.display, XKeysymToKeycode(other.display, XK_Tab), Mod1Mask,
             root_window(), True, GrabModeAsync, GrabModeAsync);
    XSelectInput(other.display, root_window(), SubstructureRedirectMask);
    other.sync();

    EXPECT_EQ(errors.errors(), 0);
}

TEST_F(ManagerTest, destructor_leaves_the_display_of_the_caller_open)
{
    start();

    manager.reset();

    const XErrors errors;
    XSync(dsp(), False);
    EXPECT_EQ(XPending(dsp()) >= 0, true);
    EXPECT_EQ(errors.errors(), 0);
    FakeClient c;
    c.map(); // not redirected any more
    EXPECT_EQ(c.attrs().map_state, IsViewable);
}

// ---- main loop ----

TEST_F(ManagerTest, new_window_is_framed_mapped_and_focused)
{
    start();
    FakeClient& c = add_client();

    run_loop();

    ASSERT_TRUE(is_managed(c));
    EXPECT_TRUE(is_framed(c));
    EXPECT_EQ(c.attrs().map_state, IsViewable);
    EXPECT_TRUE(manager->wm_windows[c.window]->is_focused());
    EXPECT_EQ(manager->wm_tops.back(), manager->wm_windows[c.window]);
}

TEST_F(ManagerTest, new_window_gets_an_icon_in_the_dock)
{
    start();
    add_client();

    run_loop();
    pump_events();

    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), 2u);
}

TEST_F(ManagerTest, focus_of_a_new_window_makes_it_active)
{
    start();
    const FakeClient& c = add_client();

    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[c.window]);
}

TEST_F(ManagerTest, the_window_of_a_dock_type_is_mapped_but_not_managed)
{
    start();
    FakeClient& c = add_client(false);
    set_window_type(c, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    c.map();

    run_loop();

    EXPECT_FALSE(is_managed(c));
    EXPECT_EQ(c.attrs().map_state, IsViewable);
    EXPECT_FALSE(is_framed(c));
}

// A dock of a client that reserves 50 px on the left and runs the loop
TEST_F(ManagerTest, a_dock_with_a_strut_shrinks_the_work_area)
{
    start();
    FakeClient& dock = add_client(false);
    set_window_type(dock, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    dock.set_cardinals(dsp()._NET_WM_STRUT, {50, 0, 0, 0});
    dock.map();

    run_loop();

    EXPECT_EQ(manager->work_area.x, 50);
    EXPECT_EQ(manager->work_area.width, screen_area().width - 50);
}

TEST_F(ManagerTest, the_work_area_is_published_on_the_root)
{
    start();

    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dsp(), root_window(), dsp()._NET_WORKAREA, 0,
                                 4, False, XA_CARDINAL, &type, &format, &count,
                                 &left, &data),
              Success);
    ASSERT_NE(data, nullptr);
    const auto* v = reinterpret_cast<const long*>(data);
    EXPECT_EQ(count, 4u);
    EXPECT_EQ(v[1], tiny::theme.wm_panel);
    EXPECT_EQ(v[2], screen_area().width);
    XFree(data);
}

TEST_F(ManagerTest, a_changed_strut_moves_the_work_area)
{
    start();
    FakeClient& dock = add_client(false);
    set_window_type(dock, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    dock.set_cardinals(dsp()._NET_WM_STRUT, {50, 0, 0, 0});
    dock.map();
    run_loop();

    dock.set_cardinals(dsp()._NET_WM_STRUT, {0, 0, 0, 80});
    run_loop();

    EXPECT_EQ(manager->work_area.x, 0);
    EXPECT_EQ(manager->work_area.height, screen_area().height - 80);
}

TEST_F(ManagerTest, a_destroyed_dock_gives_the_space_back)
{
    start();
    FakeClient& dock = add_client(false);
    set_window_type(dock, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    dock.set_cardinals(dsp()._NET_WM_STRUT, {50, 0, 0, 0});
    dock.map();
    run_loop();
    ASSERT_EQ(manager->work_area.x, 50);

    XDestroyWindow(dock.display, dock.window);
    dock.sync();
    run_loop();

    EXPECT_EQ(manager->work_area.x, 0);
    EXPECT_EQ(manager->wm_docks.size(), 0u);
}

TEST_F(ManagerTest, a_hidden_dock_reserves_nothing)
{
    start();
    FakeClient& dock = add_client(false);
    set_window_type(dock, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    dock.set_cardinals(dsp()._NET_WM_STRUT, {50, 0, 0, 0});
    dock.map();
    run_loop();

    XUnmapWindow(dock.display, dock.window);
    dock.sync();
    run_loop();

    EXPECT_EQ(manager->work_area.x, 0);
}

TEST_F(ManagerTest, maximized_windows_follow_a_dock)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    manager->wm_windows[c.window]->maximize();
    FakeClient& dock = add_client(false);
    set_window_type(dock, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    dock.set_cardinals(dsp()._NET_WM_STRUT, {50, 0, 0, 0});

    dock.map();
    run_loop();

    const XWindowAttributes f = attributes_of(parent_of(c.window));
    EXPECT_EQ(f.x, 50);
    EXPECT_EQ(f.width, screen_area().width - 50);
}

TEST_F(ManagerTest, a_new_window_is_placed_in_the_work_area)
{
    start();
    FakeClient& dock = add_client(false);
    set_window_type(dock, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    dock.set_cardinals(dsp()._NET_WM_STRUT, {50, 0, 0, 0});
    dock.map();
    run_loop();
    FakeClient& c = add_client(false);
    XMoveWindow(c.display, c.window, 0, 0);
    c.map();

    run_loop();

    const XWindowAttributes f = attributes_of(parent_of(c.window));
    EXPECT_GE(f.x, 50);
    EXPECT_GE(f.y, tiny::theme.wm_panel);
}

TEST_F(ManagerTest, window_without_decorations_is_managed_on_map_request)
{
    start();
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.map();

    run_loop();

    ASSERT_TRUE(is_managed(c));
    EXPECT_FALSE(is_framed(c));
    EXPECT_EQ(c.attrs().map_state, IsViewable);
}

TEST_F(ManagerTest, map_request_of_a_viewable_window_is_ignored)
{
    const FakeClient& c = add_client();
    start();
    ASSERT_EQ(manager->wm_windows.size(), 1u);
    XMapRequestEvent e = {};
    e.type = MapRequest;
    e.window = XCreateSimpleWindow(dsp(), root_window(), 0, 0, 10, 10, 0, 0, 0);
    XMapWindow(dsp(), e.window);
    XSync(dsp(), False);

    manager->on_map_request(e);

    EXPECT_EQ(manager->wm_windows.size(), 1u);
    XDestroyWindow(dsp(), e.window);
    (void)c;
}

TEST_F(ManagerTest, map_request_of_an_override_redirect_window_is_ignored)
{
    start();
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    XMapRequestEvent e = {};
    e.type = MapRequest;
    e.window = XCreateWindow(dsp(), root_window(), 0, 0, 10, 10, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWOverrideRedirect, &attrs);

    manager->on_map_request(e);

    EXPECT_EQ(manager->wm_windows.size(), 0u);
    XDestroyWindow(dsp(), e.window);
}

TEST_F(ManagerTest, configure_request_of_an_unmanaged_window_is_executed)
{
    start();
    FakeClient& c = add_client(false);

    XResizeWindow(c.display, c.window, 300, 150);
    c.sync();
    run_loop();

    auto attrs = c.attrs();
    EXPECT_EQ(attrs.width, 300);
    EXPECT_EQ(attrs.height, 150);
}

TEST_F(ManagerTest, configure_request_of_a_framed_window_moves_the_frame)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    ASSERT_TRUE(is_framed(c));
    const ::Window frame = parent_of(c.window);
    c.watch_structure();

    XMoveResizeWindow(c.display, c.window, 70, 120, 320, 210);
    c.sync();
    run_loop();

    const auto f = attributes_of(frame);
    EXPECT_EQ(f.x, 70 - f.border_width);
    EXPECT_EQ(f.y, 120 - tiny::theme.wm_win_header - f.border_width);
    EXPECT_EQ(f.width, 320);
    EXPECT_EQ(f.height, 210 + tiny::theme.wm_win_header);
    EXPECT_EQ(parent_of(c.window), frame);
    const auto a = c.attrs();
    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, tiny::theme.wm_win_header);
    EXPECT_EQ(a.width, 320);
    EXPECT_EQ(a.height, 210);
    XConfigureEvent notify = {};
    ASSERT_TRUE(c.synthetic_configure(notify));
    EXPECT_EQ(notify.x, 70);
}

TEST_F(ManagerTest, map_request_of_a_minimized_window_shows_it_again)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    WmWindow* window = manager->wm_windows[c.window];
    window->minimize();
    XMapRequestEvent e = {};
    e.type = MapRequest;
    e.window = c.window;

    manager->on_map_request(e);

    EXPECT_FALSE(window->get_minimized());
    EXPECT_EQ(manager->wm_windows.size(), 1u);
    EXPECT_EQ(manager->wm_tops.size(), 1u);
}

TEST_F(ManagerTest, unmapped_window_is_forgotten)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    ASSERT_TRUE(is_managed(c));
    const WmWindow* window = manager->wm_windows[c.window];
    const XErrors errors;
    const ::Window frame = parent_of(c.window);

    XUnmapWindow(c.display, c.window);
    c.sync();
    run_loop();
    pump_events();

    EXPECT_FALSE(is_managed(c));
    EXPECT_EQ(std::count(manager->wm_tops.begin(), manager->wm_tops.end(),
                         window),
              0);
    EXPECT_NE(manager->active, window);
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), 1u);
    XWindowAttributes attrs;
    EXPECT_EQ(XGetWindowAttributes(dsp(), frame, &attrs), 0); // frame is gone
    EXPECT_EQ(parent_of(c.window), root_window()); // the client is not
}

TEST_F(ManagerTest, window_hidden_by_the_client_can_be_shown_again)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    XUnmapWindow(c.display, c.window);
    c.sync();
    run_loop();
    ASSERT_FALSE(is_managed(c));

    c.map();
    run_loop();

    EXPECT_TRUE(is_managed(c));
    EXPECT_TRUE(is_framed(c));
    EXPECT_EQ(c.attrs().map_state, IsViewable);
}

TEST_F(ManagerTest, unmap_of_an_unknown_window_is_ignored)
{
    start();
    XUnmapEvent e = {};
    e.type = UnmapNotify;
    e.window = 12345;

    manager->on_unmap_notify(e);

    EXPECT_EQ(manager->wm_windows.size(), 0u);
}

TEST_F(ManagerTest, unmap_of_a_minimized_window_keeps_it)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    WmWindow* window = manager->wm_windows[c.window];
    window->minimize();
    XUnmapEvent e = {};
    e.type = UnmapNotify;
    e.window = c.window;

    manager->on_unmap_notify(e);

    EXPECT_TRUE(is_managed(c));
}

TEST_F(ManagerTest, destroyed_window_is_forgotten)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    ASSERT_TRUE(is_managed(c));
    const XErrors errors;

    XDestroyWindow(c.display, c.window);
    c.sync();
    run_loop();
    pump_events();

    EXPECT_FALSE(is_managed(c));
    EXPECT_EQ(manager->wm_tops.size(), 0u);
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), 1u);
    EXPECT_EQ(manager->active, nullptr);
}

TEST_F(ManagerTest, destroyed_minimized_window_leaves_the_dock)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    WmWindow* window = manager->wm_windows[c.window];
    window->minimize();
    ASSERT_TRUE(is_managed(c));
    const XErrors errors;

    XDestroyWindow(c.display, c.window);
    c.sync();
    run_loop();
    pump_events();

    EXPECT_FALSE(is_managed(c));
    EXPECT_EQ(manager->wm_tops.size(), 0u);
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), 1u);
    EXPECT_EQ(manager->active, nullptr);
}

TEST_F(ManagerTest, destroyed_active_window_passes_the_focus_on)
{
    manage_three();
    auto it = manager->wm_tops.begin();
    const WmWindow* first = *it++;
    WmWindow* second = *it;
    manager->active = second;
    const XErrors errors;
    ::Window gone = 0;
    for (const auto& item : manager->wm_windows) {
        if (item.second == second) {
            gone = item.first;
        }
    }
    ASSERT_NE(gone, 0u);

    XDestroyWindowEvent e = {};
    e.type = DestroyNotify;
    e.window = gone;
    manager->on_destroy_notify(e);

    EXPECT_EQ(manager->active, first);
    EXPECT_EQ(manager->wm_windows.count(gone), 0u);
}

TEST_F(ManagerTest, destroy_of_an_unknown_window_is_ignored)
{
    start();
    XDestroyWindowEvent e = {};
    e.type = DestroyNotify;
    e.window = 12345;

    manager->on_destroy_notify(e);

    EXPECT_EQ(manager->wm_windows.size(), 0u);
}

TEST_F(ManagerTest, managed_window_gets_wm_state_normal)
{
    start();
    FakeClient& c = add_client();

    run_loop();

    EXPECT_EQ(c.wm_state(), NormalState);
}

TEST_F(ManagerTest, window_hidden_by_the_client_is_withdrawn)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    c.set_state({dsp()._NET_WM_STATE_ABOVE});
    ASSERT_TRUE(c.has_property(dsp()._NET_WM_STATE));

    XUnmapWindow(c.display, c.window);
    c.sync();
    run_loop();

    EXPECT_FALSE(is_managed(c));
    EXPECT_EQ(c.wm_state(), WithdrawnState);
    EXPECT_FALSE(c.has_property(dsp()._NET_WM_STATE));
}

TEST_F(ManagerTest, the_state_of_a_window_does_not_survive_a_withdraw)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    manager->wm_windows[c.window]->maximize();
    XUnmapWindow(c.display, c.window);
    c.sync();
    run_loop();
    ASSERT_FALSE(is_managed(c));

    c.map();
    run_loop();

    ASSERT_TRUE(is_managed(c));
    EXPECT_FALSE(manager->wm_windows[c.window]->get_maximized());
}

TEST_F(ManagerTest, focus_moves_the_active_window)
{
    manage_three();
    auto it = manager->wm_tops.begin();
    WmWindow* first = *it++;
    WmWindow* second = *it;
    first->set_focus();
    ASSERT_TRUE(first->is_focused());

    second->on_focus(second, make_event(FocusIn));

    EXPECT_EQ(manager->active, second);
    EXPECT_FALSE(first->is_focused());
}

// ---- keyboard ----

TEST_F(ManagerTest, alt_f2_opens_the_run_dialog)
{
    start();

    manager->on_key_press(key(XK_F2));
    XSync(dsp(), False);

    EXPECT_EQ(attributes_of(manager->wm_run).map_state, IsViewable);
    EXPECT_TRUE(manager->key_done);
}

TEST_F(ManagerTest, super_opens_the_launcher_when_it_is_released)
{
    const EnvVar data_home("XDG_DATA_HOME", "/nonexistent");
    const EnvVar data_dirs("XDG_DATA_DIRS", "/nonexistent");
    start();
    ASSERT_NE(XKeysymToKeycode(dsp(), XK_Super_L), 0);

    manager->on_key_press(key(XK_Super_L));
    XSync(dsp(), False);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsUnmapped);
    EXPECT_TRUE(manager->key_done);

    manager->on_key_release(key(XK_Super_L));
    XSync(dsp(), False);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsViewable);

    manager->on_key_press(key(XK_Super_R)); // the second Super closes it
    manager->on_key_release(key(XK_Super_R));
    XSync(dsp(), False);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsUnmapped);
}

TEST_F(ManagerTest, super_used_to_move_a_window_does_not_open_the_launcher)
{
    const EnvVar data_home("XDG_DATA_HOME", "/nonexistent");
    const EnvVar data_dirs("XDG_DATA_DIRS", "/nonexistent");
    manage_three();
    auto* window = manager->wm_tops.front();

    manager->on_key_press(key(XK_Super_L));
    window->on_modifier_drag(window, XEvent());
    manager->on_key_release(key(XK_Super_L));
    XSync(dsp(), False);

    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsUnmapped);
}

TEST_F(ManagerTest, a_release_of_super_without_a_press_does_nothing)
{
    const EnvVar data_home("XDG_DATA_HOME", "/nonexistent");
    const EnvVar data_dirs("XDG_DATA_DIRS", "/nonexistent");
    start();

    manager->on_key_release(key(XK_Super_L));
    XSync(dsp(), False);

    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsUnmapped);
}

TEST_F(ManagerTest, the_dots_in_the_dock_toggle_the_launcher)
{
    const EnvVar data_home("XDG_DATA_HOME", "/nonexistent");
    const EnvVar data_dirs("XDG_DATA_DIRS", "/nonexistent");
    start();
    const tiny::wm::DockButton& dots = manager->wm_dock.get_menu_button();

    send(dots, ButtonRelease);
    XSync(dsp(), False);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsViewable);

    send(dots, ButtonRelease); // the second click closes it
    XSync(dsp(), False);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsUnmapped);
}

TEST_F(ManagerTest, a_window_gets_the_icon_of_its_desktop_entry)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() /
                          ("tiny-manager-icon-" + std::to_string(getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "applications");
    fs::create_directories(root / "pixmaps");
    std::ofstream(root / "pixmaps" / "epsilon.xpm")
            << "/* XPM */\nstatic char * epsilon_xpm[] = {\n"
               "\"2 2 1 1\",\n\"a c #FF0000\",\n\"aa\",\n\"aa\"};\n";
    std::ofstream(root / "applications" / "epsilon.desktop")
            << "[Desktop Entry]\nName=Epsilon\nType=Application\n"
               "Icon=epsilon\nExec=true\n";
    const EnvVar data_home("XDG_DATA_HOME", "/nonexistent");
    const EnvVar data_dirs("XDG_DATA_DIRS", root.string());
    FakeClient& known = add_client();
    known.set_class("epsilon", "Epsilon");
    FakeClient& unknown = add_client();
    unknown.set_class("zeta", "Zeta");

    start();

    EXPECT_NE(manager->wm_windows.at(known.window)->get_app_icon().picture,
              None);
    EXPECT_EQ(manager->wm_windows.at(unknown.window)->get_app_icon().picture,
              None);
    fs::remove_all(root);
}

TEST_F(ManagerTest, other_keys_are_not_handled_on_press)
{
    start();
    manager->key_done = true;

    manager->on_key_press(key(XK_a, 0));

    EXPECT_FALSE(manager->key_done);
}

TEST_F(ManagerTest, release_of_a_handled_key_is_ignored)
{
    manage_three();
    manager->key_done = true;
    auto* second = *std::next(manager->wm_tops.begin());

    XKeyEvent e = key(XK_Tab);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_FALSE(second->is_focused());
}

TEST_F(ManagerTest, alt_tab_focuses_the_next_window)
{
    manage_three();
    auto* second = *std::next(manager->wm_tops.begin());

    const XKeyEvent e = key(XK_Tab);
    manager->on_key_press(e);

    EXPECT_TRUE(second->is_focused());
}

TEST_F(ManagerTest, alt_tab_pressed_again_does_not_wait_for_the_focus_event)
{
    manage_three();
    manager->active = nullptr;
    auto it = manager->wm_tops.begin();
    const WmWindow* first = *it;
    const WmWindow* second = *++it;
    const WmWindow* third = *++it;

    manager->on_key_press(key(XK_Tab)); // no event is processed between them
    EXPECT_EQ(manager->active, first);
    manager->on_key_press(key(XK_Tab));
    EXPECT_EQ(manager->active, second);
    manager->on_key_press(key(XK_Tab));

    EXPECT_EQ(manager->active, third);
    EXPECT_TRUE(third->is_focused());
    EXPECT_FALSE(second->is_focused());
}

TEST_F(ManagerTest, alt_tab_wraps_around)
{
    manage_three();
    manager->active = manager->wm_tops.back();

    const XKeyEvent e = key(XK_Tab);
    manager->on_key_press(e);

    EXPECT_TRUE(manager->wm_tops.front()->is_focused());
}

TEST_F(ManagerTest, alt_shift_tab_focuses_the_previous_window)
{
    manage_three();
    manager->active = manager->wm_tops.back();
    auto* previous = *std::next(manager->wm_tops.rbegin());

    const XKeyEvent e = key(XK_Tab, Mod1Mask | ShiftMask);
    manager->on_key_press(e);

    EXPECT_TRUE(previous->is_focused());
}

TEST_F(ManagerTest, alt_shift_tab_wraps_around)
{
    manage_three();

    const XKeyEvent e = key(XK_Tab, Mod1Mask | ShiftMask);
    manager->on_key_press(e);

    EXPECT_TRUE(manager->wm_tops.back()->is_focused());
}

TEST_F(ManagerTest, alt_tab_restores_a_minimized_window)
{
    manage_three();
    auto* second = *std::next(manager->wm_tops.begin());
    second->minimize();
    ASSERT_TRUE(second->get_minimized());

    const XKeyEvent e = key(XK_Tab);
    manager->on_key_press(e);

    EXPECT_FALSE(second->get_minimized());
    EXPECT_TRUE(second->is_focused());
}

TEST_F(ManagerTest, alt_shift_tab_restores_a_minimized_window)
{
    manage_three();
    auto* last = manager->wm_tops.back();
    last->minimize();

    const XKeyEvent e = key(XK_Tab, Mod1Mask | ShiftMask);
    manager->on_key_press(e);

    EXPECT_FALSE(last->get_minimized());
}

TEST_F(ManagerTest, alt_tab_without_an_active_window_starts_with_the_first)
{
    manage_three();
    manager->active = nullptr;

    const XKeyEvent e = key(XK_Tab);
    manager->on_key_press(e);

    EXPECT_TRUE(manager->wm_tops.front()->is_focused());
}

TEST_F(ManagerTest, alt_tab_without_windows_does_nothing)
{
    start();

    XKeyEvent e = key(XK_Tab);
    manager->on_key_press(e);
    e.state = Mod1Mask | ShiftMask;
    manager->on_key_press(e);
}

TEST_F(ManagerTest, alt_tab_switches_even_when_alt_is_released_first)
{
    manage_three();
    auto* second = *std::next(manager->wm_tops.begin());

    manager->on_key_press(key(XK_Tab));
    XKeyEvent release = key(XK_Tab, 0); // a quick tap: Alt is already up
    release.type = KeyRelease;
    manager->on_key_release(release);

    EXPECT_TRUE(second->is_focused());
    EXPECT_TRUE(manager->key_done);
}

TEST_F(ManagerTest, unknown_key_release_is_survived)
{
    start();
    manager->key_done = false;

    XKeyEvent e = key(XK_a, 0);
    e.type = KeyRelease;
    manager->on_key_release(e);
}

// ---- errors ----

TEST_F(ManagerTest, errors_of_a_vanished_window_are_ignored_quietly)
{
    start();
    XErrorEvent error = {};
    error.display = dsp();

    for (const int code : {BadWindow, BadDrawable}) {
        error.error_code = static_cast<unsigned char>(code);
        ::testing::internal::CaptureStderr();
        EXPECT_EQ(tiny::wm::Manager::on_error(dsp(), &error), 0);
        EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");
    }
}

TEST_F(ManagerTest, other_x_errors_are_logged)
{
    start();
    XErrorEvent error = {};
    error.display = dsp();
    error.error_code = BadMatch;

    ::testing::internal::CaptureStderr();
    EXPECT_EQ(tiny::wm::Manager::on_error(dsp(), &error), 0);
    EXPECT_NE(::testing::internal::GetCapturedStderr(), "");
}

TEST_F(ManagerTest, x_errors_are_logged_and_ignored)
{
    start();
    XErrorEvent error = {};
    error.type = 0;
    error.error_code = BadWindow;
    error.display = dsp();

    EXPECT_EQ(tiny::wm::Manager::on_error(dsp(), &error), 0);
}

// ---- transient windows (dialogs) ----

TEST_F(ManagerTest, a_dialog_is_centered_over_its_parent)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();

    const XWindowAttributes p = attributes_of(parent_of(parent.window));
    const XWindowAttributes d = attributes_of(parent_of(dialog.window));
    EXPECT_EQ(d.x + d.width / 2, p.x + p.width / 2);
    EXPECT_EQ(d.y + d.height / 2, p.y + p.height / 2);
}

TEST_F(ManagerTest, a_dialog_has_no_icon_in_the_dock)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    const auto before = children_of(manager->wm_dock.get_window()).size();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();

    EXPECT_TRUE(is_managed(dialog));
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), before);
}

TEST_F(ManagerTest, a_dialog_of_an_unknown_window_is_a_normal_window)
{
    start();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(root_window());
    dialog.map();
    run_loop();

    EXPECT_TRUE(is_managed(dialog));
    EXPECT_EQ(manager->parent_of(manager->wm_windows[dialog.window]), nullptr);
}

TEST_F(ManagerTest, focusing_the_parent_raises_its_dialog)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();
    manager->wm_windows[other.window]->raise();

    manager->focus_window(manager->wm_windows[parent.window]);
    XSync(dsp(), False);

    ::Window root;
    ::Window top_parent;
    ::Window* kids = nullptr;
    unsigned int count;
    XQueryTree(dsp(), root_window(), &root, &top_parent, &kids, &count);
    const auto position = [&](const FakeClient& c) {
        return std::find(kids, kids + count, parent_of(c.window)) - kids;
    };
    EXPECT_GT(position(dialog), position(parent));
    XFree(kids);
}

TEST_F(ManagerTest, minimizing_the_parent_minimizes_and_restores_its_dialog)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    auto* parent_window = manager->wm_windows[parent.window];
    auto* dialog_window = manager->wm_windows[dialog.window];

    parent_window->minimize();
    EXPECT_TRUE(dialog_window->get_minimized());

    parent_window->restore();
    EXPECT_FALSE(dialog_window->get_minimized());
}

TEST_F(ManagerTest, closing_a_dialog_gives_the_focus_back_to_its_parent)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    manager->focus_window(manager->wm_windows[dialog.window]);
    ASSERT_EQ(manager->active, manager->wm_windows[dialog.window]);

    XDestroyWindow(dialog.display, dialog.window);
    dialog.sync();
    run_loop();

    EXPECT_EQ(manager->active, manager->wm_windows[parent.window]);
    EXPECT_NE(manager->active, manager->wm_windows[other.window]);
}

TEST_F(ManagerTest, minimizing_a_dialog_minimizes_its_parent_too)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    auto* parent_window = manager->wm_windows[parent.window];
    auto* dialog_window = manager->wm_windows[dialog.window];

    dialog_window->minimize();
    EXPECT_TRUE(parent_window->get_minimized());

    dialog_window->restore();
    EXPECT_FALSE(parent_window->get_minimized());
    EXPECT_FALSE(dialog_window->get_minimized());
}

TEST_F(ManagerTest, closing_a_window_does_not_restore_a_minimized_one)
{
    start();
    const FakeClient& hidden = add_client();
    run_loop();
    const FakeClient& closed = add_client();
    run_loop();
    manager->wm_windows[hidden.window]->minimize();
    manager->focus_window(manager->wm_windows[closed.window]);

    XDestroyWindow(closed.display, closed.window);
    closed.sync();
    run_loop();

    EXPECT_TRUE(manager->wm_windows[hidden.window]->get_minimized());
    EXPECT_EQ(manager->active, nullptr);
}

TEST_F(ManagerTest, closing_a_window_focuses_the_one_before_it)
{
    start();
    const FakeClient& first = add_client();
    run_loop();
    const FakeClient& second = add_client();
    run_loop();
    const FakeClient& third = add_client();
    run_loop();
    manager->focus_window(manager->wm_windows[third.window]);

    XDestroyWindow(third.display, third.window);
    third.sync();
    run_loop();

    EXPECT_EQ(manager->active, manager->wm_windows[second.window]);
    EXPECT_NE(manager->active, manager->wm_windows[first.window]);
}

TEST_F(ManagerTest, focusing_a_window_with_a_modal_dialog_focuses_the_dialog)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_modal();
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();
    manager->focus_window(manager->wm_windows[other.window]);
    ASSERT_EQ(manager->active, manager->wm_windows[other.window]);

    manager->focus_window(manager->wm_windows[parent.window]);

    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);
}

TEST_F(ManagerTest,
       focus_event_of_a_window_with_a_modal_dialog_moves_to_the_dialog)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_modal();
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();

    manager->on_window_focus(manager->wm_windows[parent.window], XEvent(),
                             nullptr);

    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);
}

TEST_F(ManagerTest, alt_tab_skips_a_window_that_has_a_modal_dialog)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_modal();
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();
    manager->focus_window(manager->wm_windows[other.window]);

    manager->on_key_press(key(XK_Tab));
    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);

    manager->on_key_press(key(XK_Tab));
    EXPECT_EQ(manager->active, manager->wm_windows[other.window]);

    manager->on_key_press(key(XK_Tab, Mod1Mask | ShiftMask));
    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);
}

TEST_F(ManagerTest, focusing_a_dialog_raises_its_parent_under_it)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();
    manager->wm_windows[other.window]->raise();

    manager->focus_window(manager->wm_windows[dialog.window]);
    XSync(dsp(), False);

    ::Window root;
    ::Window top_parent;
    ::Window* kids = nullptr;
    unsigned int count;
    XQueryTree(dsp(), root_window(), &root, &top_parent, &kids, &count);
    const auto position = [&](const FakeClient& c) {
        return std::find(kids, kids + count, parent_of(c.window)) - kids;
    };
    EXPECT_GT(position(parent), position(other));
    EXPECT_GT(position(dialog), position(parent));
    XFree(kids);
}

TEST_F(ManagerTest, a_transient_toolbar_is_not_treated_as_a_dialog)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    const auto before = children_of(manager->wm_dock.get_window()).size();
    FakeClient& toolbar = add_client(false);
    set_window_type(toolbar, dsp()._NET_WM_WINDOW_TYPE_TOOLBAR);
    toolbar.set_transient_for(parent.window);
    toolbar.map();
    run_loop();

    EXPECT_TRUE(
            manager->transients_of(manager->wm_windows[parent.window]).empty());
    // a toolbar is not a dialog, but it has no place in the dock either
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), before);
}

TEST_F(ManagerTest, dialogs_of_each_other_do_not_stop_alt_tab)
{
    start();
    const FakeClient& first = add_client();
    run_loop();
    const FakeClient& second = add_client();
    run_loop();
    XSetTransientForHint(first.display, first.window, second.window);
    XSetTransientForHint(second.display, second.window, first.window);
    first.sync();
    second.sync();
    manager->wm_windows[first.window]->update_transient_for();
    manager->wm_windows[second.window]->update_transient_for();

    manager->on_key_press(key(XK_Tab));

    EXPECT_NE(manager->active, nullptr);
}

} // namespace

TEST_F(ManagerTest, a_dialog_that_is_not_modal_leaves_the_focus_to_its_parent)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();
    manager->focus_window(manager->wm_windows[other.window]);

    manager->focus_window(manager->wm_windows[parent.window]);

    EXPECT_EQ(manager->active, manager->wm_windows[parent.window]);
}

TEST_F(ManagerTest, alt_tab_visits_a_dialog_that_is_not_modal_and_its_parent)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    manager->focus_window(manager->wm_windows[dialog.window]);

    manager->on_key_press(key(XK_Tab));
    EXPECT_EQ(manager->active, manager->wm_windows[parent.window]);

    manager->on_key_press(key(XK_Tab));
    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);
}

TEST_F(ManagerTest, focusing_the_parent_raises_a_dialog_that_is_not_modal)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();

    manager->focus_window(manager->wm_windows[parent.window]);

    ::Window root_return;
    ::Window parent_return;
    ::Window* children = nullptr;
    unsigned int count = 0;
    XQueryTree(manager->display, root_window(), &root_return, &parent_return,
               &children, &count);
    const auto position = [&](const FakeClient& c) {
        const ::Window frame = manager->wm_windows[c.window]->frame();
        return std::find(children, children + count, frame) - children;
    };
    EXPECT_GT(position(dialog), position(parent));
    EXPECT_GT(position(parent), position(other));
    XFree(children);
}

TEST_F(ManagerTest, a_modal_state_request_changes_the_dialog)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    auto* dialog_window = manager->wm_windows[dialog.window];
    ASSERT_TRUE(manager->modal_dialogs_of(manager->wm_windows[parent.window])
                        .empty());

    dialog_window->set_modal(true);
    EXPECT_EQ(manager->modal_dialogs_of(manager->wm_windows[parent.window]),
              std::vector<tiny::wm::Window*>{dialog_window});

    dialog_window->set_modal(false);
    EXPECT_TRUE(manager->modal_dialogs_of(manager->wm_windows[parent.window])
                        .empty());
}

TEST_F(ManagerTest, closing_the_parent_asks_its_dialogs_to_close)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.set_protocols({dsp().WM_DELETE_WINDOW});
    dialog.map();
    XSelectInput(dialog.display, dialog.window, NoEventMask);
    run_loop();
    pump_events();

    XDestroyWindow(parent.display, parent.window);
    parent.sync();
    run_loop();

    XEvent e;
    dialog.sync();
    ASSERT_TRUE(XCheckTypedEvent(dialog.display, ClientMessage, &e));
    EXPECT_EQ(static_cast<Atom>(e.xclient.data.l[0]), dsp().WM_DELETE_WINDOW);
}

TEST_F(ManagerTest, a_dialog_without_the_delete_protocol_is_not_killed)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();

    XDestroyWindow(parent.display, parent.window);
    parent.sync();
    run_loop();

    EXPECT_TRUE(is_managed(dialog));
}

TEST_F(ManagerTest, a_dialog_that_becomes_modal_takes_the_focus_from_its_parent)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    manager->focus_window(manager->wm_windows[parent.window]);
    ASSERT_EQ(manager->active, manager->wm_windows[parent.window]);

    manager->wm_windows[dialog.window]->set_modal(true);

    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);
}

TEST_F(ManagerTest, a_modal_window_without_a_parent_is_modal_for_its_group)
{
    start();
    FakeClient& first = add_client(false);
    first.set_window_group(first.window);
    first.map();
    run_loop();
    FakeClient& second = add_client(false);
    second.set_window_group(first.window);
    second.map();
    run_loop();
    FakeClient& modal = add_client(false);
    modal.set_window_group(first.window);
    modal.set_modal();
    modal.map();
    run_loop();
    const FakeClient& stranger = add_client();
    run_loop();
    manager->focus_window(manager->wm_windows[stranger.window]);

    manager->focus_window(manager->wm_windows[first.window]);
    EXPECT_EQ(manager->active, manager->wm_windows[modal.window]);

    manager->focus_window(manager->wm_windows[second.window]);
    EXPECT_EQ(manager->active, manager->wm_windows[modal.window]);

    manager->focus_window(manager->wm_windows[stranger.window]);
    EXPECT_EQ(manager->active, manager->wm_windows[stranger.window]);
}

TEST_F(ManagerTest, a_modal_window_of_a_group_does_not_block_itself)
{
    start();
    FakeClient& modal = add_client(false);
    modal.set_window_group(modal.window);
    modal.set_modal();
    modal.map();
    run_loop();

    manager->focus_window(manager->wm_windows[modal.window]);

    EXPECT_EQ(manager->active, manager->wm_windows[modal.window]);
}

TEST_F(ManagerTest, a_modal_window_without_a_group_blocks_nothing)
{
    start();
    const FakeClient& other = add_client();
    run_loop();
    FakeClient& modal = add_client(false);
    modal.set_modal();
    modal.map();
    run_loop();

    manager->focus_window(manager->wm_windows[other.window]);

    EXPECT_EQ(manager->active, manager->wm_windows[other.window]);
}

TEST_F(ManagerTest, a_modal_transient_blocks_only_its_own_parent)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    const FakeClient& other = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.set_modal();
    dialog.map();
    run_loop();

    manager->focus_window(manager->wm_windows[other.window]);

    EXPECT_EQ(manager->active, manager->wm_windows[other.window]);
}

TEST_F(ManagerTest, a_group_that_becomes_modal_takes_the_focus)
{
    start();
    FakeClient& first = add_client(false);
    first.set_window_group(first.window);
    first.map();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_window_group(first.window);
    dialog.map();
    run_loop();
    manager->focus_window(manager->wm_windows[first.window]);
    ASSERT_EQ(manager->active, manager->wm_windows[first.window]);

    manager->wm_windows[dialog.window]->set_modal(true);

    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);
}

// ---- EWMH: stacking list, frame extents, window state ----

TEST_F(ManagerTest, the_stacking_list_has_the_lowest_window_first)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();

    manager->focus_window(manager->wm_windows[first.window]);
    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING,
                                       XA_WINDOW)),
              (std::vector<::Window>{second.window, first.window}));

    manager->focus_window(manager->wm_windows[second.window]);
    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING,
                                       XA_WINDOW)),
              (std::vector<::Window>{first.window, second.window}));
    // the list in the order of mapping does not change
    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST, XA_WINDOW)),
              (std::vector<::Window>{first.window, second.window}));
}

TEST_F(ManagerTest, the_stacking_list_loses_a_window_that_is_gone)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();

    XUnmapWindow(first.display, first.window);
    first.sync();
    run_loop();
    pump_events();

    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING,
                                       XA_WINDOW)),
              (std::vector<::Window>{second.window}));
}

TEST_F(ManagerTest, the_stacking_list_is_removed_when_the_manager_ends)
{
    start();
    add_client();
    run_loop();
    ASSERT_FALSE(
            root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING, XA_WINDOW)
                    .empty());

    manager->clear_published();

    EXPECT_TRUE(
            root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING, XA_WINDOW)
                    .empty());
}

namespace {

std::vector<long> frame_extents_of(const FakeClient& c)
{
    c.sync();
    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    std::vector<long> values;
    if (XGetWindowProperty(c.display, c.window, dsp()._NET_FRAME_EXTENTS, 0, 4,
                           False, XA_CARDINAL, &type, &format, &count, &left,
                           &data) == Success &&
        data) {
        const auto* first = reinterpret_cast<const long*>(data);
        values.assign(first, first + count);
        XFree(data);
    }
    return values;
}

} // namespace

TEST_F(ManagerTest, a_framed_window_gets_its_frame_extents)
{
    start();
    const FakeClient& c = add_client();
    run_loop();

    const tiny::wm::FrameExtents e =
            manager->wm_windows[c.window]->frame_extents();
    EXPECT_EQ(frame_extents_of(c),
              (std::vector<long>{e.left, e.right, e.top, e.bottom}));
    EXPECT_EQ(e.top, tiny::theme.wm_win_header + e.bottom);
}

TEST_F(ManagerTest, a_window_without_a_frame_has_no_extents)
{
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.map();
    start();
    run_loop();

    EXPECT_EQ(frame_extents_of(c), (std::vector<long>{0, 0, 0, 0}));
}

TEST_F(ManagerTest, a_window_can_ask_for_its_extents_before_it_is_mapped)
{
    start();
    const FakeClient& c = add_client(false);
    ASSERT_TRUE(frame_extents_of(c).empty());

    request(c.window, dsp()._NET_REQUEST_FRAME_EXTENTS);
    run_loop();

    const tiny::wm::FrameExtents e = tiny::wm::Client::default_frame_extents();
    EXPECT_EQ(frame_extents_of(c),
              (std::vector<long>{e.left, e.right, e.top, e.bottom}));
}

TEST_F(ManagerTest, the_frame_extents_follow_maximize_and_restore)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    const auto normal = frame_extents_of(c);

    manager->wm_windows[c.window]->maximize();
    EXPECT_EQ(frame_extents_of(c),
              (std::vector<long>{0, 0, tiny::theme.wm_win_header, 0}));

    manager->wm_windows[c.window]->restore(0, 0);
    EXPECT_EQ(frame_extents_of(c), normal);
}

TEST_F(ManagerTest, a_window_without_decorations_is_told_it_has_no_frame)
{
    start();
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);

    request(c.window, dsp()._NET_REQUEST_FRAME_EXTENTS);
    run_loop();

    EXPECT_EQ(frame_extents_of(c), (std::vector<long>{0, 0, 0, 0}));
}

TEST_F(ManagerTest, a_utility_window_has_no_icon_in_the_dock)
{
    start();
    const auto before = children_of(manager->wm_dock.get_window()).size();
    FakeClient& c = add_client(false);
    set_window_type(c, dsp()._NET_WM_WINDOW_TYPE_UTILITY);
    c.map();
    run_loop();

    EXPECT_TRUE(is_managed(c));
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), before);
}

TEST_F(ManagerTest, skip_taskbar_removes_the_icon_from_the_dock_and_back)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    auto* window = manager->wm_windows[c.window];
    const auto with_icon = children_of(manager->wm_dock.get_window()).size();
    ASSERT_TRUE(manager->wm_dock.contains(window));

    window->set_skip_taskbar(true);
    EXPECT_FALSE(manager->wm_dock.contains(window));
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), with_icon - 1);

    window->set_skip_taskbar(false);
    EXPECT_TRUE(manager->wm_dock.contains(window));
    EXPECT_EQ(children_of(manager->wm_dock.get_window()).size(), with_icon);
}

TEST_F(ManagerTest, skip_taskbar_does_not_add_a_dialog_to_the_dock)
{
    start();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    auto* window = manager->wm_windows[dialog.window];

    window->set_skip_taskbar(true);
    window->set_skip_taskbar(false);

    EXPECT_FALSE(manager->wm_dock.contains(window));
}

TEST_F(ManagerTest, a_window_above_stays_over_the_focused_one)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    manager->wm_windows[first.window]->set_above(true);

    manager->focus_window(manager->wm_windows[second.window]);

    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING,
                                       XA_WINDOW)),
              (std::vector<::Window>{second.window, first.window}));
}

TEST_F(ManagerTest, a_window_below_stays_under_the_others)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    manager->focus_window(manager->wm_windows[first.window]);

    manager->wm_windows[first.window]->set_below(true);

    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING,
                                       XA_WINDOW)),
              (std::vector<::Window>{first.window, second.window}));
}

TEST_F(ManagerTest, a_window_that_starts_above_is_on_top)
{
    FakeClient& first = add_client(false);
    first.set_state({dsp()._NET_WM_STATE_ABOVE});
    first.map();
    start();
    const FakeClient& second = add_client();
    run_loop();

    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST_STACKING,
                                       XA_WINDOW)),
              (std::vector<::Window>{second.window, first.window}));
}

TEST_F(ManagerTest, the_supported_list_has_what_the_manager_handles)
{
    start();

    const auto atoms = root_property<Atom>(dsp()._NET_SUPPORTED, XA_ATOM);

    for (const Atom atom :
         {dsp()._NET_CLIENT_LIST_STACKING, dsp()._NET_FRAME_EXTENTS,
          dsp()._NET_REQUEST_FRAME_EXTENTS, dsp()._NET_WM_STATE_ABOVE,
          dsp()._NET_WM_STATE_BELOW, dsp()._NET_WM_STATE_DEMANDS_ATTENTION}) {
        EXPECT_NE(std::count(atoms.begin(), atoms.end(), atom), 0);
    }
}

TEST_F(ManagerTest, a_minimized_window_keeps_its_icon_when_it_skips_the_taskbar)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    auto* window = manager->wm_windows[c.window];
    window->minimize();

    window->set_skip_taskbar(true);

    EXPECT_TRUE(manager->wm_dock.contains(window));
}

TEST_F(ManagerTest, a_window_that_skips_the_taskbar_loses_the_icon_on_restore)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    auto* window = manager->wm_windows[c.window];
    window->minimize();
    window->set_skip_taskbar(true);

    window->restore();

    EXPECT_FALSE(manager->wm_dock.contains(window));
}

TEST_F(ManagerTest, a_window_that_skips_the_taskbar_gets_an_icon_to_minimize)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    auto* window = manager->wm_windows[c.window];
    window->set_skip_taskbar(true);
    ASSERT_FALSE(manager->wm_dock.contains(window));

    window->minimize();

    EXPECT_TRUE(manager->wm_dock.contains(window));
}
