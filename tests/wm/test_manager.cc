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
void request(::Window window, Atom message, long data0 = 0, long data1 = 0)
{
    XEvent e = {};
    e.xclient.type = ClientMessage;
    e.xclient.window = window;
    e.xclient.message_type = message;
    e.xclient.format = 32;
    e.xclient.data.l[0] = data0;
    e.xclient.data.l[1] = data1;
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
    EXPECT_NE(std::count(atoms.begin(), atoms.end(), dsp()._NET_WM_PING), 0);
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

TEST_F(ManagerTest, an_old_request_of_an_application_only_marks_the_window)
{
    start();
    FakeClient& first = add_client(false);
    FakeClient& second = add_client(false);
    first.set_user_time(5000);
    second.set_user_time(6000);
    first.map();
    second.map();
    run_loop();
    pump_events();
    ASSERT_EQ(manager->active, manager->wm_windows[second.window]);

    request(first.window, dsp()._NET_ACTIVE_WINDOW, 1, 4000);
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[second.window]);
    EXPECT_TRUE(manager->wm_windows[first.window]->demands_attention());
}

TEST_F(ManagerTest, a_request_of_an_application_without_a_time_only_marks)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();

    request(first.window, dsp()._NET_ACTIVE_WINDOW, 1, 0);
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[second.window]);
    EXPECT_TRUE(manager->wm_windows[first.window]->demands_attention());
}

TEST_F(ManagerTest, a_recent_request_of_an_application_focuses_the_window)
{
    start();
    FakeClient& first = add_client(false);
    FakeClient& second = add_client(false);
    first.set_user_time(5000);
    second.set_user_time(6000);
    first.map();
    second.map();
    run_loop();
    pump_events();

    request(first.window, dsp()._NET_ACTIVE_WINDOW, 1, 7000);
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);
}

TEST_F(ManagerTest, an_old_request_of_a_pager_still_focuses_the_window)
{
    start();
    FakeClient& first = add_client(false);
    FakeClient& second = add_client(false);
    first.set_user_time(5000);
    second.set_user_time(6000);
    first.map();
    second.map();
    run_loop();
    pump_events();

    request(first.window, dsp()._NET_ACTIVE_WINDOW, 2, 4000);
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);
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

TEST_F(ManagerTest, the_dock_is_redrawn_when_a_window_demands_attention)
{
    start();
    const FakeClient& c = add_client();
    const FakeClient& other = add_client();
    run_loop();
    pump_events();
    auto* window = manager->wm_windows[c.window];
    ASSERT_FALSE(window->is_focused());
    ASSERT_NE(other.window, None);

    window->set_demands_attention(true);

    EXPECT_TRUE(window->demands_attention());
    window->set_focus();
    EXPECT_FALSE(window->demands_attention());
}

TEST_F(ManagerTest, a_minimized_window_is_not_published_as_active)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();

    manager->wm_windows[c.window]->minimize();

    EXPECT_EQ((root_property<::Window>(dsp()._NET_ACTIVE_WINDOW, XA_WINDOW)),
              (std::vector<::Window>{None}));
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

TEST_F(ManagerTest, the_echo_of_a_ping_is_taken_from_the_root)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    WmWindow* window = manager->wm_windows[c.window];
    const auto now = WmWindow::Clock::now();
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);
    ASSERT_FALSE(window->is_responding());

    // the window is named in data[2]
    XEvent e = {};
    e.xclient.type = ClientMessage;
    e.xclient.window = root_window();
    e.xclient.message_type = dsp().WM_PROTOCOLS;
    e.xclient.format = 32;
    e.xclient.data.l[0] = static_cast<long>(dsp()._NET_WM_PING);
    e.xclient.data.l[2] = static_cast<long>(c.window);
    XSendEvent(dsp(), root_window(), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);
    run_loop();

    EXPECT_TRUE(window->is_responding());
}

TEST_F(ManagerTest, the_loop_wakes_up_for_the_first_overdue_ping)
{
    start();
    FakeClient& c = add_client();
    FakeClient& d = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    d.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    EXPECT_EQ(manager->ping_timeout_ms(), -1);

    const auto now = WmWindow::Clock::now();
    manager->wm_windows[c.window]->ping(now);
    manager->wm_windows[d.window]->ping(now - std::chrono::seconds(1));

    const int ms = manager->ping_timeout_ms();
    EXPECT_GT(ms, 3000);
    EXPECT_LE(ms, 4000);
    manager->expire_pings(); // nothing is overdue yet
    EXPECT_TRUE(manager->wm_windows[c.window]->is_responding());
}

namespace {

// `_NET_WM_DESKTOP` of a client as a 32 bit value (Xlib extends the sign)
unsigned long desktop_of(FakeClient& c)
{
    const auto values = c.cardinals(dsp()._NET_WM_DESKTOP);
    return values.size() == 1 ? values[0] & 0xFFFFFFFFUL : 12345;
}

} // namespace

// ---- _NET_WM_MOVERESIZE ----

namespace {

// A client asks the window manager to move or resize its window.
void request_move_resize(::Window window, long direction, long x = 100,
                         long y = 100)
{
    XEvent e = {};
    e.xclient.type = ClientMessage;
    e.xclient.window = window;
    e.xclient.message_type = dsp()._NET_WM_MOVERESIZE;
    e.xclient.format = 32;
    e.xclient.data.l[0] = x;
    e.xclient.data.l[1] = y;
    e.xclient.data.l[2] = direction;
    e.xclient.data.l[3] = 1;
    XSendEvent(dsp(), root_window(), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XSync(dsp(), False);
}

} // namespace

TEST_F(ManagerTest, a_client_can_ask_to_be_moved_with_the_pointer)
{
    start();
    manager->is_button_down = []() { return true; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    const WmWindow* window = manager->wm_windows[c.window];
    const XWindowAttributes before = attributes_of(window->frame());

    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER, 100, 100);
    run_loop();
    ASSERT_EQ(manager->dragged, window);
    manager->move_resize_to(130, 150);

    const XWindowAttributes after = attributes_of(window->frame());
    EXPECT_EQ(after.x, before.x + 30);
    EXPECT_EQ(after.y, before.y + 50);
    EXPECT_EQ(after.width, before.width);
}

TEST_F(ManagerTest, a_client_can_ask_to_be_resized_with_the_pointer)
{
    start();
    manager->is_button_down = []() { return true; };
    FakeClient& c = add_client();
    run_loop();
    pump_events();
    const WmWindow* window = manager->wm_windows[c.window];
    const XWindowAttributes before = attributes_of(window->frame());

    request_move_resize(c.window, tiny::wm::SIZE_BOTTOMRIGHT, 100, 100);
    run_loop();
    ASSERT_EQ(manager->dragged, window);
    manager->move_resize_to(140, 170);

    const XWindowAttributes after = attributes_of(window->frame());
    EXPECT_EQ(after.x, before.x);
    EXPECT_EQ(after.y, before.y);
    EXPECT_EQ(after.width, before.width + 40);
    EXPECT_EQ(after.height, before.height + 70);
    EXPECT_EQ(c.attrs().width, before.width + 40);
}

TEST_F(ManagerTest, a_window_without_a_frame_is_resized_by_the_pointer)
{
    start();
    manager->is_button_down = []() { return true; };
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.map();
    run_loop();
    pump_events();
    ASSERT_FALSE(is_framed(c));
    const XWindowAttributes before = c.attrs();

    request_move_resize(c.window, tiny::wm::SIZE_TOPLEFT, 100, 100);
    run_loop();
    ASSERT_NE(manager->dragged, nullptr);
    manager->move_resize_to(90, 80); // 10 and 20 pixels outside

    const XWindowAttributes after = c.attrs();
    EXPECT_EQ(after.x, before.x - 10);
    EXPECT_EQ(after.y, before.y - 20);
    EXPECT_EQ(after.width, 210);
    EXPECT_EQ(after.height, 120);
}

TEST_F(ManagerTest, a_window_without_a_frame_is_moved_by_the_pointer)
{
    start();
    manager->is_button_down = []() { return true; };
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.map();
    run_loop();
    pump_events();
    const XWindowAttributes before = c.attrs();

    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER, 100, 100);
    run_loop();
    manager->move_resize_to(110, 125);

    EXPECT_EQ(c.attrs().x, before.x + 10);
    EXPECT_EQ(c.attrs().y, before.y + 25);
}

TEST_F(ManagerTest, nothing_is_dragged_when_no_button_is_down)
{
    start();
    manager->is_button_down = []() { return false; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();

    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();

    EXPECT_EQ(manager->dragged, nullptr);
}

TEST_F(ManagerTest, the_keyboard_variants_are_not_supported)
{
    start();
    manager->is_button_down = []() { return true; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();

    request_move_resize(c.window, tiny::wm::SIZE_KEYBOARD);
    request_move_resize(c.window, tiny::wm::MOVE_KEYBOARD);
    request_move_resize(c.window, 99);
    run_loop();

    EXPECT_EQ(manager->dragged, nullptr);
}

TEST_F(ManagerTest, a_window_that_may_not_resize_is_not_resized_by_a_client)
{
    start();
    manager->is_button_down = []() { return true; };
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                tiny::wm::MotifWMHints::FUNC_MOVE, 0xFFFF);
    c.map();
    run_loop();
    pump_events();

    request_move_resize(c.window, tiny::wm::SIZE_RIGHT);
    run_loop();
    EXPECT_EQ(manager->dragged, nullptr);

    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();
    EXPECT_NE(manager->dragged, nullptr);
}

TEST_F(ManagerTest, a_window_that_may_not_move_is_not_moved_by_a_client)
{
    start();
    manager->is_button_down = []() { return true; };
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                tiny::wm::MotifWMHints::FUNC_RESIZE, 0xFFFF);
    c.map();
    run_loop();
    pump_events();

    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();

    EXPECT_EQ(manager->dragged, nullptr);
}

TEST_F(ManagerTest, a_maximized_window_is_moved_but_not_resized_by_a_client)
{
    start();
    manager->is_button_down = []() { return true; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    manager->wm_windows[c.window]->maximize();

    request_move_resize(c.window, tiny::wm::SIZE_BOTTOM);
    run_loop();
    EXPECT_EQ(manager->dragged, nullptr);

    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER, 300, 20);
    run_loop();
    EXPECT_NE(manager->dragged, nullptr);
    EXPECT_FALSE(manager->wm_windows[c.window]->get_maximized());
}

TEST_F(ManagerTest, a_button_let_go_before_the_grab_starts_no_drag)
{
    start();
    int checks = 0;
    manager->is_button_down = [&checks]() { return ++checks == 1; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();

    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();

    EXPECT_EQ(manager->dragged, nullptr);
    EXPECT_EQ(checks, 2);
}

TEST_F(ManagerTest, the_client_can_cancel_the_drag)
{
    start();
    manager->is_button_down = []() { return true; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();
    ASSERT_NE(manager->dragged, nullptr);

    request_move_resize(c.window, tiny::wm::MOVE_RESIZE_CANCEL);
    run_loop();

    EXPECT_EQ(manager->dragged, nullptr);
}

TEST_F(ManagerTest, the_pointer_events_of_the_grab_drive_the_window)
{
    start();
    manager->is_button_down = []() { return true; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    const WmWindow* window = manager->wm_windows[c.window];
    const XWindowAttributes before = attributes_of(window->frame());
    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER, 100, 100);
    run_loop();
    ASSERT_EQ(manager->dragged, window);

    // the events of a grab come to the root; here they are sent by hand
    XSelectInput(dsp(), root_window(),
                 SubstructureNotifyMask | SubstructureRedirectMask |
                         PointerMotionMask | ButtonReleaseMask);
    XEvent motion = make_event(MotionNotify);
    motion.xmotion.window = root_window();
    motion.xmotion.x_root = 120;
    motion.xmotion.y_root = 110;
    XSendEvent(dsp(), root_window(), False, PointerMotionMask, &motion);
    run_loop();
    EXPECT_EQ(attributes_of(window->frame()).x, before.x + 20);
    EXPECT_EQ(attributes_of(window->frame()).y, before.y + 10);

    XEvent release = make_event(ButtonRelease);
    release.xbutton.window = root_window();
    release.xbutton.button = Button2; // not the button of the drag
    XSendEvent(dsp(), root_window(), False, ButtonReleaseMask, &release);
    run_loop();
    EXPECT_EQ(manager->dragged, window);
    release.xbutton.button = Button1;
    XSendEvent(dsp(), root_window(), False, ButtonReleaseMask, &release);
    run_loop();
    EXPECT_EQ(manager->dragged, nullptr);
}

TEST_F(ManagerTest, the_client_is_told_its_place_when_the_drag_ends)
{
    start();
    manager->is_button_down = []() { return true; };
    FakeClient& c = add_client();
    run_loop();
    pump_events();
    c.watch_structure();
    const WmWindow* window = manager->wm_windows[c.window];
    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER, 100, 100);
    run_loop();
    manager->move_resize_to(150, 160);

    manager->stop_move_resize();

    XConfigureEvent notify;
    ASSERT_TRUE(c.synthetic_configure(notify));
    const XWindowAttributes frame = attributes_of(window->frame());
    EXPECT_EQ(notify.x, frame.x + frame.border_width + c.attrs().x);
    EXPECT_EQ(notify.y, frame.y + frame.border_width + c.attrs().y);
}

TEST_F(ManagerTest,
       a_window_that_goes_away_during_the_drag_releases_the_pointer)
{
    start();
    manager->is_button_down = []() { return true; };
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    request_move_resize(c.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();
    ASSERT_NE(manager->dragged, nullptr);

    manager->unmanage(c.window, true);

    EXPECT_EQ(manager->dragged, nullptr);
    // the pointer is free again: another drag can start
    const FakeClient& d = add_client();
    run_loop();
    pump_events();
    request_move_resize(d.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();
    EXPECT_NE(manager->dragged, nullptr);
}

TEST_F(ManagerTest, a_new_drag_replaces_the_old_one)
{
    start();
    manager->is_button_down = []() { return true; };
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();
    request_move_resize(first.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();

    request_move_resize(second.window, tiny::wm::MOVE_BY_POINTER);
    run_loop();

    EXPECT_EQ(manager->dragged, manager->wm_windows[second.window]);
}

TEST_F(ManagerTest, the_default_check_sees_that_no_button_is_down)
{
    start();

    EXPECT_FALSE(manager->is_button_down());
}

TEST_F(ManagerTest, a_managed_window_gets_its_allowed_actions)
{
    start();
    FakeClient& c = add_client();
    run_loop();

    const auto actions = c.atoms_of(dsp()._NET_WM_ALLOWED_ACTIONS);

    EXPECT_EQ(actions, manager->wm_windows[c.window]->get_allowed_actions());
    EXPECT_EQ(std::count(actions.begin(), actions.end(),
                         dsp()._NET_WM_ACTION_SHADE),
              1);
}

TEST_F(ManagerTest, the_allowed_actions_follow_the_motif_functions)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    pump_events();

    c.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                tiny::wm::MotifWMHints::FUNC_ALL |
                        tiny::wm::MotifWMHints::FUNC_CLOSE,
                0);
    run_loop();

    const auto actions = c.atoms_of(dsp()._NET_WM_ALLOWED_ACTIONS);
    EXPECT_EQ(std::count(actions.begin(), actions.end(),
                         dsp()._NET_WM_ACTION_CLOSE),
              0);
    EXPECT_EQ(std::count(actions.begin(), actions.end(),
                         dsp()._NET_WM_ACTION_MOVE),
              1);
}

TEST_F(ManagerTest, a_fixed_size_takes_the_resize_from_the_allowed_actions)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    pump_events();

    c.set_size_hints(PMinSize | PMaxSize, 100, 100, 100, 100);
    run_loop();

    const auto actions = c.atoms_of(dsp()._NET_WM_ALLOWED_ACTIONS);
    EXPECT_EQ(std::count(actions.begin(), actions.end(),
                         dsp()._NET_WM_ACTION_RESIZE),
              0);
    EXPECT_EQ(std::count(actions.begin(), actions.end(),
                         dsp()._NET_WM_ACTION_MAXIMIZE_HORZ),
              0);
}

TEST_F(ManagerTest, the_allowed_actions_are_removed_when_the_client_leaves)
{
    start();
    FakeClient& c = add_client();
    run_loop();

    manager->unmanage(c.window, true);

    EXPECT_FALSE(c.has_property(dsp()._NET_WM_ALLOWED_ACTIONS));
}

TEST_F(ManagerTest, the_one_desktop_is_published)
{
    start();

    EXPECT_EQ((root_property<unsigned long>(dsp()._NET_NUMBER_OF_DESKTOPS,
                                            XA_CARDINAL)),
              (std::vector<unsigned long>{1}));
    EXPECT_EQ((root_property<unsigned long>(dsp()._NET_CURRENT_DESKTOP,
                                            XA_CARDINAL)),
              (std::vector<unsigned long>{0}));
    EXPECT_EQ((root_property<unsigned long>(dsp()._NET_DESKTOP_VIEWPORT,
                                            XA_CARDINAL)),
              (std::vector<unsigned long>{0, 0}));
    const XWindowAttributes screen = attributes_of(root_window());
    EXPECT_EQ((root_property<unsigned long>(dsp()._NET_DESKTOP_GEOMETRY,
                                            XA_CARDINAL)),
              (std::vector<unsigned long>{
                      static_cast<unsigned long>(screen.width),
                      static_cast<unsigned long>(screen.height)}));
}

TEST_F(ManagerTest, the_desktop_properties_go_away_with_the_manager)
{
    start();

    manager.reset();

    EXPECT_TRUE((root_property<unsigned long>(dsp()._NET_NUMBER_OF_DESKTOPS,
                                              XA_CARDINAL))
                        .empty());
    EXPECT_TRUE((root_property<unsigned long>(dsp()._NET_CURRENT_DESKTOP,
                                              XA_CARDINAL))
                        .empty());
}

TEST_F(ManagerTest, a_managed_window_is_on_the_desktop_zero)
{
    start();
    FakeClient& c = add_client();
    run_loop();

    EXPECT_EQ(desktop_of(c), 0u);
}

TEST_F(ManagerTest, a_window_that_asks_for_all_desktops_is_sticky)
{
    start();
    FakeClient& c = add_client(false);
    c.set_cardinals(dsp()._NET_WM_DESKTOP, {0xFFFFFFFFL});
    c.map();
    run_loop();

    EXPECT_TRUE(manager->wm_windows[c.window]->is_sticky());
    EXPECT_TRUE(c.state_has(dsp()._NET_WM_STATE_STICKY));
    EXPECT_EQ(desktop_of(c), 0xFFFFFFFFUL);
}

TEST_F(ManagerTest, the_sticky_state_is_changed_by_requests)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    pump_events();

    request(c.window, dsp()._NET_WM_STATE, tiny::Display::_NET_WM_STATE_ADD,
            static_cast<long>(dsp()._NET_WM_STATE_STICKY));
    run_loop();
    EXPECT_TRUE(manager->wm_windows[c.window]->is_sticky());
    EXPECT_EQ(desktop_of(c), 0xFFFFFFFFUL);

    request(c.window, dsp()._NET_WM_STATE, tiny::Display::_NET_WM_STATE_TOGGLE,
            static_cast<long>(dsp()._NET_WM_STATE_STICKY));
    run_loop();
    EXPECT_FALSE(manager->wm_windows[c.window]->is_sticky());
    EXPECT_EQ(desktop_of(c), 0u);
}

TEST_F(ManagerTest, a_pager_moves_a_window_to_all_desktops_or_the_only_one)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    const WmWindow* window = manager->wm_windows[c.window];

    request(c.window, dsp()._NET_WM_DESKTOP, 0xFFFFFFFFL);
    run_loop();
    EXPECT_TRUE(window->is_sticky());

    request(c.window, dsp()._NET_WM_DESKTOP, 3); // there is no such desktop
    run_loop();
    EXPECT_TRUE(window->is_sticky());

    request(c.window, dsp()._NET_WM_DESKTOP, 0);
    run_loop();
    EXPECT_FALSE(window->is_sticky());
}

TEST_F(ManagerTest, a_pager_cannot_add_desktops_or_switch_to_another)
{
    start();

    request(root_window(), dsp()._NET_NUMBER_OF_DESKTOPS, 4);
    request(root_window(), dsp()._NET_CURRENT_DESKTOP, 2);
    run_loop();

    EXPECT_EQ((root_property<unsigned long>(dsp()._NET_NUMBER_OF_DESKTOPS,
                                            XA_CARDINAL)),
              (std::vector<unsigned long>{1}));
    EXPECT_EQ((root_property<unsigned long>(dsp()._NET_CURRENT_DESKTOP,
                                            XA_CARDINAL)),
              (std::vector<unsigned long>{0}));
}

TEST_F(ManagerTest, skip_pager_is_changed_by_requests)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    pump_events();

    request(c.window, dsp()._NET_WM_STATE, tiny::Display::_NET_WM_STATE_ADD,
            static_cast<long>(dsp()._NET_WM_STATE_SKIP_PAGER));
    run_loop();

    EXPECT_TRUE(manager->wm_windows[c.window]->skip_pager());
    EXPECT_TRUE(c.state_has(dsp()._NET_WM_STATE_SKIP_PAGER));
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

TEST_F(ManagerTest, the_panel_reserves_its_edge_with_a_strut)
{
    start();

    const tiny::wm::Struts strut =
            tiny::wm::read_struts(manager->wm_panel.get_window());

    EXPECT_EQ(strut.top, manager->wm_panel.get_height());
    EXPECT_EQ(strut.left + strut.right + strut.bottom, 0);
    EXPECT_EQ(manager->work_area.y, strut.top);
}

TEST_F(ManagerTest, the_strut_of_the_panel_spans_the_whole_width)
{
    start();
    ::Display* dpy = tiny::get_display();

    Atom type;
    int format;
    unsigned long items;
    unsigned long after;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dpy, manager->wm_panel.get_window(),
                                 tiny::get_display()._NET_WM_STRUT_PARTIAL, 0,
                                 12, False, XA_CARDINAL, &type, &format, &items,
                                 &after, &data),
              Success);
    ASSERT_EQ(items, 12u);
    const auto* v = reinterpret_cast<const long*>(data);
    const long top_start_x = v[8];
    const long top_end_x = v[9];
    XFree(data);
    XWindowAttributes screen;
    XGetWindowAttributes(dpy, DefaultRootWindow(dpy), &screen);
    EXPECT_EQ(top_start_x, 0);
    EXPECT_EQ(top_end_x, screen.width - 1);
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

// ---- focus stealing prevention ----

TEST_F(ManagerTest, a_new_window_without_user_time_takes_the_focus)
{
    start();
    const FakeClient& first = add_client();
    run_loop();
    pump_events();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[second.window]);
    EXPECT_NE(first.window, second.window);
}

TEST_F(ManagerTest, a_new_window_with_user_time_zero_does_not_take_the_focus)
{
    start();
    const FakeClient& first = add_client();
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time(0);
    second.map();
    run_loop();
    pump_events();

    auto* window = manager->wm_windows[second.window];
    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);
    EXPECT_TRUE(window->demands_attention());
}

TEST_F(ManagerTest, a_window_older_than_the_active_one_does_not_steal_the_focus)
{
    start();
    FakeClient& first = add_client(false);
    first.set_user_time(5000);
    first.map();
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time(4000);
    second.map();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);
}

TEST_F(ManagerTest, a_window_denied_the_focus_stays_under_the_active_one)
{
    start();
    FakeClient& first = add_client(false);
    first.set_user_time(5000);
    first.map();
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time(4000);
    second.map();
    run_loop();
    pump_events();

    const std::vector<::Window> order = children_of(root_window());
    const ::Window active_frame = manager->wm_windows[first.window]->frame();
    const ::Window denied_frame = manager->wm_windows[second.window]->frame();
    ASSERT_NE(std::find(order.begin(), order.end(), active_frame), order.end());
    EXPECT_LT(std::find(order.begin(), order.end(), denied_frame),
              std::find(order.begin(), order.end(), active_frame));
}

TEST_F(ManagerTest, the_user_time_window_of_a_new_window_is_read)
{
    start();
    FakeClient& first = add_client(false);
    first.set_user_time(5000);
    first.map();
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time_window(4000);
    second.map();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);
}

TEST_F(ManagerTest, a_newer_time_on_the_user_time_window_takes_the_focus)
{
    start();
    FakeClient& first = add_client(false);
    first.set_user_time(5000);
    first.map();
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time_window(6000);
    second.map();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[second.window]);
}

TEST_F(ManagerTest, the_time_of_the_user_time_window_is_the_time_of_the_user)
{
    start();
    FakeClient& first = add_client(false);
    first.set_user_time(1000);
    first.map();
    run_loop();
    pump_events();
    const ::Window time_window = first.set_user_time_window(9000);
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time(8000);
    second.map();
    run_loop();
    pump_events();
    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);

    const unsigned long later = 9500;
    XChangeProperty(first.display, time_window, dsp()._NET_WM_USER_TIME,
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&later), 1);
    first.sync();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->last_user_time, later);
}

TEST_F(ManagerTest, a_window_newer_than_the_active_one_takes_the_focus)
{
    start();
    FakeClient& first = add_client(false);
    first.set_user_time(5000);
    first.map();
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time(6000);
    second.map();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[second.window]);
}

TEST_F(ManagerTest, the_active_window_moves_the_time_of_the_user_forward)
{
    start();
    FakeClient& first = add_client(false);
    first.set_user_time(5000);
    first.map();
    run_loop();
    pump_events();
    first.set_user_time(8000); // a key press in the window
    run_loop();
    pump_events();
    FakeClient& second = add_client(false);
    second.set_user_time(7000);
    second.map();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[first.window]);
}

TEST_F(ManagerTest, a_dialog_of_the_active_window_takes_the_focus)
{
    start();
    FakeClient& parent = add_client(false);
    parent.set_user_time(5000);
    parent.map();
    run_loop();
    pump_events();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.set_user_time(0);
    dialog.map();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->active, manager->wm_windows[dialog.window]);
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

    manager->on_window_focus(manager->wm_windows[parent.window], XEvent());

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

TEST_F(ManagerTest, the_frame_extents_ask_for_the_decorations_of_the_hints)
{
    start();
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0,
                tiny::wm::MotifWMHints::DECOR_TITLE);

    request(c.window, dsp()._NET_REQUEST_FRAME_EXTENTS);
    run_loop();

    EXPECT_EQ(frame_extents_of(c),
              (std::vector<long>{0, 0, tiny::theme.wm_win_header, 0}));
}

TEST_F(ManagerTest, a_window_without_a_frame_gets_one_when_the_hints_change)
{
    start();
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.map();
    run_loop();
    pump_events();
    ASSERT_FALSE(is_framed(c));

    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0,
                tiny::wm::MotifWMHints::DECOR_ALL);
    run_loop();
    pump_events();

    EXPECT_TRUE(is_managed(c));
    EXPECT_TRUE(is_framed(c));
    EXPECT_EQ(frame_extents_of(c),
              (std::vector<long>{
                      tiny::wm::Client::default_frame_extents().left,
                      tiny::wm::Client::default_frame_extents().right,
                      tiny::wm::Client::default_frame_extents().top,
                      tiny::wm::Client::default_frame_extents().bottom}));
}

TEST_F(ManagerTest, a_framed_window_loses_the_frame_when_the_hints_change)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    pump_events();
    ASSERT_TRUE(is_framed(c));

    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    run_loop();
    pump_events();

    EXPECT_TRUE(is_managed(c));
    EXPECT_FALSE(is_framed(c));
    EXPECT_EQ(parent_of(c.window), root_window());
    EXPECT_EQ(attributes_of(c.window).map_state, IsViewable);
    EXPECT_EQ(frame_extents_of(c), (std::vector<long>{0, 0, 0, 0}));
}

TEST_F(ManagerTest, a_reframed_window_keeps_the_focus_and_its_state)
{
    start();
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.set_state({dsp()._NET_WM_STATE_ABOVE});
    c.map();
    run_loop();
    pump_events();
    ASSERT_EQ(manager->active, manager->wm_windows[c.window]);

    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0,
                tiny::wm::MotifWMHints::DECOR_ALL);
    run_loop();
    pump_events();

    ASSERT_TRUE(is_managed(c));
    EXPECT_EQ(manager->active, manager->wm_windows[c.window]);
    EXPECT_TRUE(manager->wm_windows[c.window]->is_above());
}

TEST_F(ManagerTest, a_reframed_window_stays_managed_after_the_unmap_events)
{
    start();
    FakeClient& c = add_client(false);
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    c.map();
    run_loop();
    pump_events();

    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0,
                tiny::wm::MotifWMHints::DECOR_ALL);
    run_loop();
    pump_events();
    run_loop(); // the unmap of the reparenting arrives meanwhile
    pump_events();

    EXPECT_TRUE(is_managed(c));
    EXPECT_EQ((root_property<::Window>(dsp()._NET_CLIENT_LIST, XA_WINDOW)),
              (std::vector<::Window>{c.window}));
}

TEST_F(ManagerTest, a_reframed_window_stays_minimized)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    pump_events();
    manager->wm_windows[c.window]->minimize();

    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    run_loop();
    pump_events();

    ASSERT_TRUE(is_managed(c));
    EXPECT_TRUE(manager->wm_windows[c.window]->get_minimized());
}

TEST_F(ManagerTest, a_reframe_keeps_the_dialogs_of_the_window)
{
    start();
    FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();
    pump_events();

    parent.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    run_loop();
    pump_events();

    XEvent message;
    EXPECT_TRUE(is_managed(parent));
    EXPECT_TRUE(is_managed(dialog));
    EXPECT_FALSE(dialog.message(message)); // nobody asked it to close
}

TEST_F(ManagerTest, a_window_that_starts_shaded_is_rolled_up)
{
    start();
    FakeClient& c = add_client(false);
    c.set_state({dsp()._NET_WM_STATE_SHADED});
    c.map();
    run_loop();
    pump_events();

    auto* window = manager->wm_windows[c.window];
    EXPECT_TRUE(window->is_shaded());
    EXPECT_EQ(attributes_of(window->frame()).height, tiny::theme.wm_win_header);
    window->shade(false);
    EXPECT_GT(attributes_of(window->frame()).height, tiny::theme.wm_win_header);
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

namespace {

// True when `lower` is under `upper` among the children of the root.
bool stacked_under(::Window lower, ::Window upper)
{
    const auto stack = children_of(root_window());
    const auto lower_at = std::find(stack.begin(), stack.end(), lower);
    const auto upper_at = std::find(stack.begin(), stack.end(), upper);
    return lower_at != stack.end() && upper_at != stack.end() &&
           lower_at < upper_at;
}

} // namespace

TEST_F(ManagerTest, the_panel_and_the_dock_stay_over_the_normal_windows)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();

    manager->focus_window(manager->wm_windows[first.window]);

    for (const FakeClient* c : {&first, &second}) {
        const ::Window frame = manager->wm_windows[c->window]->frame();
        EXPECT_TRUE(stacked_under(frame, manager->wm_panel.get_window()));
        EXPECT_TRUE(stacked_under(frame, manager->wm_dock.get_window()));
    }
}

TEST_F(ManagerTest, a_window_above_is_over_the_panel_and_the_dock)
{
    start();
    const FakeClient& c = add_client();
    run_loop();

    manager->wm_windows[c.window]->set_above(true);

    const ::Window frame = manager->wm_windows[c.window]->frame();
    EXPECT_TRUE(stacked_under(manager->wm_panel.get_window(), frame));
    EXPECT_TRUE(stacked_under(manager->wm_dock.get_window(), frame));
}

TEST_F(ManagerTest, a_fullscreen_window_is_over_a_window_above_and_the_panel)
{
    start();
    const FakeClient& above = add_client();
    const FakeClient& full = add_client();
    run_loop();
    manager->wm_windows[above.window]->set_above(true);

    manager->wm_windows[full.window]->set_fullscreen(true);
    manager->focus_window(manager->wm_windows[above.window]);

    const ::Window full_frame = manager->wm_windows[full.window]->frame();
    EXPECT_TRUE(stacked_under(manager->wm_panel.get_window(), full_frame));
    EXPECT_TRUE(stacked_under(manager->wm_windows[above.window]->frame(),
                              full_frame));
}

TEST_F(ManagerTest, the_dock_of_a_client_is_over_the_normal_windows)
{
    start();
    FakeClient& dock = add_client(false);
    set_window_type(dock, dsp()._NET_WM_WINDOW_TYPE_DOCK);
    dock.map();
    const FakeClient& c = add_client();
    run_loop();

    manager->focus_window(manager->wm_windows[c.window]);

    EXPECT_TRUE(
            stacked_under(manager->wm_windows[c.window]->frame(), dock.window));
}

TEST_F(ManagerTest, a_dialog_of_a_window_below_goes_down_with_it)
{
    start();
    const FakeClient& other = add_client();
    const FakeClient& parent = add_client();
    run_loop();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();

    manager->wm_windows[parent.window]->set_below(true);

    const ::Window other_frame = manager->wm_windows[other.window]->frame();
    const ::Window parent_frame = manager->wm_windows[parent.window]->frame();
    const ::Window dialog_frame = manager->wm_windows[dialog.window]->frame();
    EXPECT_TRUE(stacked_under(dialog_frame, other_frame));
    EXPECT_TRUE(stacked_under(parent_frame, dialog_frame));
}

TEST_F(ManagerTest, a_desktop_window_stays_under_a_window_below)
{
    start();
    FakeClient& desktop = add_client(false);
    set_window_type(desktop, dsp()._NET_WM_WINDOW_TYPE_DESKTOP);
    desktop.map();
    const FakeClient& c = add_client();
    run_loop();

    manager->wm_windows[c.window]->set_below(true);

    const auto stack = children_of(root_window());
    const auto desktop_at =
            std::find(stack.begin(), stack.end(), desktop.window);
    const auto frame_at = std::find(stack.begin(), stack.end(),
                                    manager->wm_windows[c.window]->frame());
    ASSERT_NE(desktop_at, stack.end());
    ASSERT_NE(frame_at, stack.end());
    EXPECT_LT(desktop_at, frame_at);
}

TEST_F(ManagerTest, a_window_that_starts_fullscreen_covers_the_screen)
{
    start();
    FakeClient& c = add_client(false);
    c.set_state({dsp()._NET_WM_STATE_FULLSCREEN});
    c.map();
    run_loop();

    auto* window = manager->wm_windows[c.window];
    const XWindowAttributes screen = attributes_of(root_window());
    const XWindowAttributes frame = attributes_of(window->frame());
    EXPECT_TRUE(window->is_fullscreen());
    EXPECT_EQ(frame.width, screen.width);
    EXPECT_EQ(frame.height, screen.height);
    EXPECT_EQ(frame_extents_of(c), (std::vector<long>{0, 0, 0, 0}));
}

TEST_F(ManagerTest, a_window_that_starts_maximized_fills_the_work_area)
{
    start();
    FakeClient& c = add_client(false);
    c.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                 dsp()._NET_WM_STATE_MAXIMIZED_HORZ});
    c.map();
    run_loop();

    auto* window = manager->wm_windows[c.window];
    const XWindowAttributes frame = attributes_of(window->frame());
    EXPECT_TRUE(window->get_maximized());
    EXPECT_EQ(frame.width, manager->work_area.width);
    EXPECT_EQ(frame.height, manager->work_area.height);
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

TEST_F(ManagerTest, leaves_when_another_manager_takes_the_selection)
{
    start();
    ::Display* display = tiny::get_display();
    tiny::wm::Selection selection(display, root_window(),
                                  XDefaultScreen(display));
    selection.acquire(false, std::chrono::milliseconds(100));
    manager->set_selection(&selection);

    XSelectionClearEvent e = {};
    e.selection = XInternAtom(display, "WM_S0", False);
    e.window = XGetSelectionOwner(display, e.selection);
    manager->running = true;
    manager->on_selection_clear(e);
    manager->set_selection(nullptr); // the selection ends with the test

    EXPECT_FALSE(manager->running);
}

TEST_F(ManagerTest, other_selections_do_not_stop_it)
{
    start();
    ::Display* display = tiny::get_display();
    tiny::wm::Selection selection(display, root_window(),
                                  XDefaultScreen(display));
    selection.acquire(false, std::chrono::milliseconds(100));
    manager->set_selection(&selection);

    XSelectionClearEvent e = {};
    e.selection = XInternAtom(display, "CLIPBOARD", False);
    e.window =
            XGetSelectionOwner(display, XInternAtom(display, "WM_S0", False));
    manager->running = true;
    manager->on_selection_clear(e);
    manager->set_selection(nullptr); // the selection ends with the test

    EXPECT_TRUE(manager->running);
}

TEST_F(ManagerTest, a_manager_without_selection_ignores_the_clear)
{
    start();
    const XSelectionClearEvent e = {};
    manager->running = true;
    manager->on_selection_clear(e);

    EXPECT_TRUE(manager->running);
}
