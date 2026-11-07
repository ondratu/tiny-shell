#include "fake_client.h"
#include "wm_helpers.h"
#include "wm/shell_theme.h"

#include <X11/keysym.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <csignal>
#include <chrono>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "wm/dock.h"
#ifdef TINY_RANDR
#include <X11/extensions/Xrandr.h>
#endif
#include "wm/panel.h"
#include "wm/process.h"
#include "wm/reload.h"
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
    return tiny::wm::shell_theme.dock_border;
}

// Stops the main loop: the loop reaches it after all earlier events.
class Stopper : public tiny::Object {
  public:
    Stopper(bool* running_arg): running(running_arg) {}

    void on_event(const XEvent&, void*) { *running = false; }

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
        tiny::handlers.set_handler(
                CreateNotify, sentinel,
                [&stopper](const XEvent& e) { stopper.on_event(e, nullptr); });
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
        manager->wm_recent.touch(manager->active);
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
    EXPECT_EQ(dock.x, root.width / 2 - dock.width / 2);
    EXPECT_EQ(dock.y, root.height - manager->wm_dock.get_height() - border());
}

TEST_F(ManagerTest, connects_logout_of_the_main_menu)
{
    start();
    ASSERT_TRUE(manager->running);

    auto& logout = manager->wm_panel.menu_main.logout;
    logout.on_click(&logout, make_event(ButtonRelease));

    EXPECT_FALSE(manager->running);
}

TEST_F(ManagerTest, the_actions_of_the_shell_are_registered_by_name)
{
    start();

    const std::vector<std::string> names = manager->get_actions().names();
    for (const char* name :
         {"about", "settings", "terminal", "force-quit", "restart", "shutdown",
          "logout", "applications", "launcher", "run-dialog", "next-window",
          "prev-window", "show-desktop", "theme-dark", "theme-light",
          "theme-toggle"}) {
        EXPECT_NE(std::find(names.begin(), names.end(), name), names.end())
                << name;
    }
}

TEST_F(ManagerTest, an_action_can_end_the_manager_and_open_a_dialog)
{
    start();

    EXPECT_TRUE(manager->get_actions().run("run-dialog"));
    EXPECT_TRUE(manager->wm_run.get_maped());

    EXPECT_TRUE(manager->get_actions().run("logout"));
    EXPECT_FALSE(manager->running);
}

TEST_F(ManagerTest, the_launcher_action_toggles_and_applications_only_opens)
{
    start();

    manager->get_actions().run("launcher");
    EXPECT_TRUE(manager->wm_launcher.is_open());
    manager->get_actions().run("applications");
    EXPECT_TRUE(manager->wm_launcher.is_open());
    manager->get_actions().run("launcher");
    EXPECT_FALSE(manager->wm_launcher.is_open());
}

TEST_F(ManagerTest, the_window_actions_walk_through_the_recent_windows)
{
    manage_three();
    const WmWindow* first = manager->active;
    const WmWindow* last = manager->wm_tops.back();

    manager->get_actions().run("next-window");
    EXPECT_NE(manager->active, first);

    manager->get_actions().run("next-window"); // back, not on to the third
    EXPECT_EQ(manager->active, first);

    manager->get_actions().run("prev-window"); // the one used the longest ago
    EXPECT_EQ(manager->active, last);
}

namespace {

// Puts the dark variant back, whatever a test did
class VariantGuard {
  public:
    ~VariantGuard() { tiny::wm::shell_theme.set_variant(tiny::Variant::Dark); }
};

} // namespace

TEST_F(ManagerTest, the_theme_actions_change_the_colors_of_the_shown_widgets)
{
    const VariantGuard guard;
    start();
    const uint32_t dark = tiny::theme.widget.normal_bg;
    run_loop();
    ASSERT_EQ(Image(manager->wm_panel.get_window(), 20, 5).at(2, 2), dark);

    manager->get_actions().run("theme-light");
    run_loop();

    EXPECT_EQ(tiny::theme.get_variant(), tiny::Variant::Light);
    EXPECT_NE(tiny::theme.widget.normal_bg, dark);
    EXPECT_EQ(Image(manager->wm_panel.get_window(), 20, 5).at(2, 2),
              tiny::theme.widget.normal_bg);

    manager->get_actions().run("theme-toggle");
    run_loop();

    EXPECT_EQ(tiny::theme.get_variant(), tiny::Variant::Dark);
    EXPECT_EQ(Image(manager->wm_panel.get_window(), 20, 5).at(2, 2), dark);

    manager->get_actions().run("theme-dark");
    EXPECT_EQ(tiny::theme.get_variant(), tiny::Variant::Dark);
}

TEST_F(ManagerTest, the_frames_of_the_windows_follow_the_variant)
{
    const VariantGuard guard;
    add_client();
    start();
    run_loop();
    ASSERT_EQ(manager->wm_tops.size(), 1u);

    manager->set_variant(tiny::Variant::Light);
    run_loop();

    // the frame is repainted without an error, and the panel got the colors
    EXPECT_EQ(tiny::theme.get_variant(), tiny::Variant::Light);
    EXPECT_EQ(manager->wm_tops.size(), 1u);
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

TEST_F(ManagerTest, restart_of_the_main_menu_runs_its_command)
{
    start();
    const auto marker = marker_file("tiny-manager-restart-");
    manager->restart_command = "touch " + marker.string();

    auto& restart = manager->wm_panel.menu_main.restart;
    restart.on_click(&restart, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    EXPECT_TRUE(manager->running); // only the command is started
    std::filesystem::remove(marker);
}

TEST_F(ManagerTest, shutdown_of_the_main_menu_runs_its_command)
{
    start();
    const auto marker = marker_file("tiny-manager-shutdown-");
    manager->shutdown_command = "touch " + marker.string();

    auto& shutdown = manager->wm_panel.menu_main.shutdown;
    shutdown.on_click(&shutdown, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
}

TEST_F(ManagerTest, an_empty_command_of_the_main_menu_does_nothing)
{
    start();
    manager->shutdown_command.clear();

    auto& shutdown = manager->wm_panel.menu_main.shutdown;
    shutdown.on_click(&shutdown, make_event(ButtonRelease));

    EXPECT_TRUE(manager->running);
}

TEST_F(ManagerTest, a_missing_command_of_the_main_menu_is_reported)
{
    start();
    manager->restart_command = "tiny-no-such-program";

    ::testing::internal::CaptureStderr();
    auto& restart = manager->wm_panel.menu_main.restart;
    restart.on_click(&restart, make_event(ButtonRelease));

    EXPECT_NE(::testing::internal::GetCapturedStderr().find(
                      "tiny-no-such-program"),
              std::string::npos);
}

namespace {

//! An executable that makes the marker file, so a program (started without a
//! shell and without arguments) leaves a trace
std::filesystem::path marker_program(const std::filesystem::path& marker)
{
    auto program = marker;
    program += ".sh";
    std::ofstream(program) << "#!/bin/sh\ntouch '" << marker.string() << "'\n";
    std::filesystem::permissions(program, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::add);
    return program;
}

} // namespace

TEST_F(ManagerTest, about_of_the_main_menu_starts_the_about_program)
{
    start();
    const auto marker = marker_file("tiny-manager-about-");
    const auto program = marker_program(marker);
    manager->set_about_command(program.string());

    auto& about = manager->wm_panel.menu_main.about;
    about.on_click(&about, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    EXPECT_TRUE(manager->running);
    std::filesystem::remove(marker);
    std::filesystem::remove(program);
}

TEST_F(ManagerTest, settings_of_the_main_menu_starts_the_settings_program)
{
    start();
    const auto marker = marker_file("tiny-manager-settings-");
    const auto program = marker_program(marker);
    manager->set_settings_command(program.string());

    auto& settings = manager->wm_panel.menu_main.settings;
    settings.on_click(&settings, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    EXPECT_TRUE(manager->running);
    std::filesystem::remove(marker);
    std::filesystem::remove(program);
}

TEST_F(ManagerTest,
       terminal_of_the_main_menu_starts_the_program_of_the_terminal)
{
    start();
    const auto marker = marker_file("tiny-manager-terminal-");
    const auto program = marker_program(marker);
    manager->set_terminal({program.string(), "-e"}); // "-e" is dropped

    auto& terminal = manager->wm_panel.menu_main.terminal;
    terminal.on_click(&terminal, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
    std::filesystem::remove(program);
}

TEST_F(ManagerTest, force_quit_of_the_main_menu_starts_xkill)
{
    start();
    const auto marker = marker_file("tiny-manager-xkill-");
    const auto program = marker_program(marker);
    manager->force_quit_program = program.string();

    auto& force_quit = manager->wm_panel.menu_main.force_quit;
    force_quit.on_click(&force_quit, make_event(ButtonRelease));

    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
    std::filesystem::remove(program);
}

TEST_F(ManagerTest, a_missing_program_of_the_main_menu_is_reported)
{
    start();
    manager->set_settings_command("tiny-no-such-settings");

    ::testing::internal::CaptureStderr();
    auto& settings = manager->wm_panel.menu_main.settings;
    settings.on_click(&settings, make_event(ButtonRelease));

    EXPECT_NE(::testing::internal::GetCapturedStderr().find(
                      "tiny-no-such-settings"),
              std::string::npos);
}

TEST_F(ManagerTest, no_terminal_of_the_main_menu_does_nothing)
{
    start();
    manager->set_terminal({});

    auto& terminal = manager->wm_panel.menu_main.terminal;
    terminal.on_click(&terminal, make_event(ButtonRelease));

    EXPECT_TRUE(manager->running);
}

TEST_F(ManagerTest, an_item_of_the_main_menu_closes_the_menu)
{
    start();
    auto& menu = manager->wm_panel.menu_main;
    menu.popup(10, 10);
    ASSERT_EQ(attributes_of(menu).map_state, IsViewable);

    manager->set_terminal({});
    menu.terminal.on_click(&menu.terminal, make_event(ButtonRelease));

    EXPECT_EQ(attributes_of(menu).map_state, IsUnmapped);
}

TEST_F(ManagerTest, applications_of_the_main_menu_opens_the_launcher)
{
    start();
    ASSERT_FALSE(manager->wm_launcher.is_open());

    auto& applications = manager->wm_panel.menu_main.applications;
    applications.on_click(&applications, make_event(ButtonRelease));

    EXPECT_TRUE(manager->wm_launcher.is_open());
}

TEST_F(ManagerTest, signals_end_the_main_loop)
{
    // SIGHUP ends it only when nothing can read the settings again
    for (const int number : {SIGTERM, SIGHUP, SIGINT}) {
        start();
        ASSERT_TRUE(manager->running);

        raise(number);
        manager->main_loop();

        EXPECT_FALSE(manager->running) << "signal " << number;
        manager.reset();
    }
}

TEST_F(ManagerTest, sighup_reads_the_settings_again_and_goes_on)
{
    start();
    int reloaded = 0;
    manager->set_reload([&] { ++reloaded; });
    raise(SIGHUP); // the loop handles it first and goes on
    std::thread stop([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        raise(SIGTERM);
    });

    manager->main_loop();
    stop.join();

    EXPECT_EQ(reloaded, 1);
    EXPECT_FALSE(manager->running); // it was SIGTERM that ended it
}

TEST_F(ManagerTest, reload_finds_the_manager_and_signals_it)
{
    start();
    int reloaded = 0;
    manager->set_reload([&] {
        ++reloaded;
        manager->running = false;
    });

    EXPECT_EQ(tiny::wm::request_reload(tiny::get_display(), root_window()), "");
    manager->main_loop();

    EXPECT_EQ(reloaded, 1);
}

TEST_F(ManagerTest, reload_says_why_it_can_not_signal)
{
    // nobody
    EXPECT_NE(tiny::wm::request_reload(tiny::get_display(), root_window()), "");

    // another window manager: it has the window of the check, but its name
    FakeClient other;
    const Atom check = dsp()._NET_SUPPORTING_WM_CHECK;
    XChangeProperty(other.display, root_window(), check, XA_WINDOW, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&other.window), 1);
    other.sync();
    EXPECT_EQ(tiny::wm::request_reload(tiny::get_display(), root_window()),
              "the window manager that runs is not tiny-shell");
    XDeleteProperty(other.display, root_window(), check);
    other.sync();
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

    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), 3u);
}

TEST_F(ManagerTest, windows_that_skip_the_taskbar_have_no_dock_icon)
{
    FakeClient& c = add_client();
    c.set_state({dsp()._NET_WM_STATE_SKIP_TASKBAR});

    start();
    pump_events();

    EXPECT_TRUE(is_managed(c));
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), 1u);
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

// The windows that menus, tooltips and notifications use: the client knows
// where they go, so they get no frame, no focus, no place in the dock and
// stay above the windows
TEST_F(ManagerTest, popups_are_not_adopted_and_stay_where_they_were_put)
{
    for (const Atom type :
         {dsp()._NET_WM_WINDOW_TYPE_NOTIFICATION,
          dsp()._NET_WM_WINDOW_TYPE_POPUP_MENU,
          dsp()._NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
          dsp()._NET_WM_WINDOW_TYPE_TOOLTIP, dsp()._NET_WM_WINDOW_TYPE_COMBO,
          dsp()._NET_WM_WINDOW_TYPE_DND}) {
        FakeClient& c = add_client(false);
        set_window_type(c, type);
        c.map();
    }
    start();

    EXPECT_EQ(manager->wm_windows.size(), 0u);
    EXPECT_EQ(manager->wm_popups.size(), 6u);
    for (const auto& c : clients) {
        const XWindowAttributes attrs = attributes_of(c->window);
        EXPECT_EQ(parent_of(c->window), root_window()); // no frame
        EXPECT_EQ(attrs.map_state, IsViewable);         // shown
        EXPECT_EQ(attrs.x, 10);                         // not moved
        EXPECT_EQ(attrs.y, 20);
        EXPECT_EQ(attrs.border_width, 0); // no border
    }
}

TEST_F(ManagerTest, a_popup_never_takes_the_focus)
{
    const FakeClient& window = add_client();
    FakeClient& tip = add_client(false);
    set_window_type(tip, dsp()._NET_WM_WINDOW_TYPE_TOOLTIP);
    start();
    manager->focus_window(manager->wm_windows[window.window]);
    ASSERT_EQ(manager->active, manager->wm_windows[window.window]);

    tip.map();
    run_loop();

    EXPECT_EQ(manager->wm_popups.count(tip.window), 1u);
    EXPECT_EQ(attributes_of(tip.window).map_state, IsViewable);
    EXPECT_EQ(manager->active, manager->wm_windows[window.window]);
    EXPECT_TRUE(manager->wm_windows[window.window]->is_focused());
    EXPECT_EQ(manager->wm_windows.size(), 1u); // nor is it in Alt+Tab
}

TEST_F(ManagerTest, a_popup_is_above_the_windows_also_after_a_focus_change)
{
    const FakeClient& first = add_client();
    FakeClient& menu = add_client(false);
    set_window_type(menu, dsp()._NET_WM_WINDOW_TYPE_POPUP_MENU);
    start();
    menu.map();
    run_loop();
    ASSERT_EQ(manager->wm_popups.count(menu.window), 1u);

    manager->focus_window(manager->wm_windows[first.window]);

    // the last of the children is the top one
    XSync(dsp(), False);
    ::Window ignored;
    ::Window parent;
    ::Window* children = nullptr;
    unsigned int count = 0;
    XQueryTree(dsp(), root_window(), &ignored, &parent, &children, &count);
    ASSERT_GT(count, 0u);
    std::vector<::Window> order(children, children + count);
    XFree(children);
    const auto popup = std::find(order.begin(), order.end(), menu.window);
    const auto frame =
            std::find(order.begin(), order.end(), parent_of(first.window));
    ASSERT_NE(popup, order.end());
    ASSERT_NE(frame, order.end());
    EXPECT_GT(popup, frame);
}

TEST_F(ManagerTest, a_popup_that_is_hidden_or_destroyed_is_forgotten)
{
    FakeClient& tip = add_client(false);
    set_window_type(tip, dsp()._NET_WM_WINDOW_TYPE_TOOLTIP);
    start();
    tip.map();
    run_loop();
    ASSERT_EQ(manager->wm_popups.size(), 1u);

    XUnmapWindow(tip.display, tip.window);
    tip.sync();
    run_loop();
    EXPECT_EQ(manager->wm_popups.size(), 0u);

    tip.map(); // shown again: it is found again
    run_loop();
    EXPECT_EQ(manager->wm_popups.size(), 1u);

    XDestroyWindow(tip.display, tip.window);
    tip.sync();
    run_loop();
    EXPECT_EQ(manager->wm_popups.size(), 0u);
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
}

TEST_F(ManagerTest, window_with_decorations_is_framed)
{
    const FakeClient& c = add_client();

    start();

    ASSERT_TRUE(is_managed(c));
    EXPECT_TRUE(is_framed(c));
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
    EXPECT_NE(
            std::count(atoms.begin(), atoms.end(), dsp()._NET_WM_SYNC_REQUEST),
            0);
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

// ---- _NET_SHOWING_DESKTOP ----

namespace {

std::vector<unsigned long> showing_desktop()
{
    return root_property<unsigned long>(dsp()._NET_SHOWING_DESKTOP,
                                        XA_CARDINAL);
}

} // namespace

TEST_F(ManagerTest, the_one_desktop_has_a_name)
{
    start();

    const auto names =
            root_property<char>(dsp()._NET_DESKTOP_NAMES, dsp().UTF8_STRING);

    // one name for one desktop, ended by a zero byte
    EXPECT_EQ(std::string(names.begin(), names.end()),
              std::string("Desktop\0", 8));
}

TEST_F(ManagerTest, the_name_of_the_desktop_is_removed_at_the_end)
{
    start();

    manager.reset();

    EXPECT_TRUE(root_property<char>(dsp()._NET_DESKTOP_NAMES, dsp().UTF8_STRING)
                        .empty());
}

TEST_F(ManagerTest, the_desktop_is_not_shown_at_the_start)
{
    start();

    EXPECT_EQ(showing_desktop(), (std::vector<unsigned long>{0}));
}

TEST_F(ManagerTest,
       showing_the_desktop_hides_all_the_windows_and_brings_them_back)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();
    auto* hidden_before = manager->wm_windows[first.window];
    hidden_before->minimize();

    request(root_window(), dsp()._NET_SHOWING_DESKTOP, 1);
    run_loop();
    pump_events();

    EXPECT_EQ(showing_desktop(), (std::vector<unsigned long>{1}));
    EXPECT_TRUE(manager->wm_windows[first.window]->get_minimized());
    EXPECT_TRUE(manager->wm_windows[second.window]->get_minimized());

    request(root_window(), dsp()._NET_SHOWING_DESKTOP, 0);
    run_loop();
    pump_events();

    EXPECT_EQ(showing_desktop(), (std::vector<unsigned long>{0}));
    EXPECT_FALSE(manager->wm_windows[second.window]->get_minimized());
    // it was minimized before, so the desktop does not bring it back
    EXPECT_TRUE(manager->wm_windows[first.window]->get_minimized());
}

TEST_F(ManagerTest, focusing_a_window_ends_showing_the_desktop)
{
    start();
    const FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();
    manager->get_actions().run("show-desktop");
    ASSERT_TRUE(manager->wm_windows[first.window]->get_minimized());

    request(first.window, dsp()._NET_ACTIVE_WINDOW, 2);
    run_loop();
    pump_events();

    EXPECT_EQ(showing_desktop(), (std::vector<unsigned long>{0}));
    EXPECT_FALSE(manager->wm_windows[first.window]->get_minimized());
    EXPECT_FALSE(manager->wm_windows[second.window]->get_minimized());
}

TEST_F(ManagerTest, the_show_desktop_action_toggles_and_forgets_closed_windows)
{
    start();
    FakeClient& first = add_client();
    const FakeClient& second = add_client();
    run_loop();
    pump_events();

    manager->get_actions().run("show-desktop");
    EXPECT_TRUE(manager->showing_desktop);
    XUnmapWindow(first.display, first.window);
    first.sync();
    run_loop();
    pump_events();
    manager->get_actions().run("show-desktop");

    EXPECT_FALSE(manager->showing_desktop);
    EXPECT_FALSE(manager->wm_windows[second.window]->get_minimized());
}

TEST_F(ManagerTest, showing_the_desktop_without_windows_only_sets_the_property)
{
    start();

    request(root_window(), dsp()._NET_SHOWING_DESKTOP, 1);
    run_loop();

    EXPECT_EQ(showing_desktop(), (std::vector<unsigned long>{1}));
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

    const auto now = WmWindow::Clock::now();
    manager->wm_windows[c.window]->pong();
    manager->wm_windows[d.window]->pong();
    manager->wm_windows[c.window]->ping(now);
    manager->wm_windows[d.window]->ping(now - std::chrono::seconds(1));

    const int ms = manager->ping_timeout_ms();
    EXPECT_GT(ms, 3000);
    EXPECT_LE(ms, 4000);
    manager->check_pings(); // nothing is overdue yet
    EXPECT_TRUE(manager->wm_windows[c.window]->is_responding());
}

TEST_F(ManagerTest, the_loop_sleeps_when_no_client_is_pinged)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    ASSERT_TRUE(is_managed(c));

    EXPECT_EQ(manager->ping_timeout_ms(), -1);
}

TEST_F(ManagerTest, clients_are_pinged_while_they_live)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    XEvent e;
    ASSERT_TRUE(c.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp()._NET_WM_PING));
    WmWindow* window = manager->wm_windows[c.window];

    manager->check_pings(); // the answer is awaited
    EXPECT_FALSE(c.message(e));
    window->pong();
    EXPECT_EQ(manager->ping_timeout_ms() > 0, true);
}

namespace {

// A client that does not respond and whose turn to be asked has come
WmWindow* make_unresponsive(WmWindow* window)
{
    const auto now = WmWindow::Clock::now();
    window->pong(); // forget the ping of the loop
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);
    window->postpone_ask(now - WmWindow::wait_delay - std::chrono::seconds(1));
    return window;
}

// When the user is to be asked, the beginning of time for a window that responds
WmWindow::Clock::time_point ask_time(const WmWindow& window)
{
    return window.get_ask_time().value_or(WmWindow::Clock::time_point{});
}

// The button of the dialog: wait is the third child, force quit the fourth
void click_dialog_button(::Window dialog, size_t index)
{
    const ::Window button = children_of(dialog).at(index);
    send(button, make_event(EnterNotify));
    send(button, make_event(ButtonRelease));
}

} // namespace

TEST_F(ManagerTest, the_user_is_asked_about_a_client_that_does_not_respond)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsUnmapped);
    const WmWindow* window = make_unresponsive(manager->wm_windows[c.window]);

    manager->check_pings();

    EXPECT_EQ(manager->asked, window);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsViewable);
}

TEST_F(ManagerTest, a_client_that_responds_is_not_asked_about)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();

    manager->check_pings();

    EXPECT_EQ(manager->asked, nullptr);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsUnmapped);
}

TEST_F(ManagerTest, closing_a_client_that_does_not_respond_asks_at_once)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp().WM_DELETE_WINDOW, dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    WmWindow* window = manager->wm_windows[c.window];
    const auto now = WmWindow::Clock::now();
    window->pong();
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);

    window->close();

    EXPECT_EQ(manager->asked, window);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsViewable);
}

TEST_F(ManagerTest, waiting_hides_the_dialog_and_asks_again_later)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    const WmWindow* window = make_unresponsive(manager->wm_windows[c.window]);
    manager->check_pings();

    click_dialog_button(manager->wm_unresponsive.get_window(), 2);

    EXPECT_EQ(manager->asked, nullptr);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsUnmapped);
    EXPECT_GT(ask_time(*window),
              WmWindow::Clock::now() + WmWindow::wait_delay / 2);
    manager->check_pings();
    EXPECT_EQ(manager->asked, nullptr); // not yet
    EXPECT_FALSE(window->is_responding());
}

TEST_F(ManagerTest, quitting_kills_the_process_of_the_client)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    const pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        alarm(30); // never outlives a failed test
        pause();
        _exit(0);
    }
    c.set_cardinals(dsp()._NET_WM_PID, {pid});
    std::string host = tiny::wm::local_host_name();
    char* names[] = {host.data()};
    XTextProperty text;
    ASSERT_NE(XStringListToTextProperty(names, 1, &text), 0);
    XSetWMClientMachine(c.display, c.window, &text);
    XFree(text.value);
    c.sync();
    run_loop();
    pump_events();
    make_unresponsive(manager->wm_windows[c.window]);
    manager->check_pings();

    click_dialog_button(manager->wm_unresponsive.get_window(), 3);

    // the manager may reap its children itself (then there is nothing to wait
    // for), so wait until the process is gone one way or the other
    int status = 0;
    pid_t gone = 0;
    for (int i = 0; i < 200 && gone == 0; ++i) {
        gone = waitpid(pid, &status, WNOHANG);
        if (gone == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    EXPECT_NE(gone, 0);
    if (gone == pid) {
        EXPECT_TRUE(WIFSIGNALED(status));
    }
    EXPECT_EQ(manager->asked, nullptr);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsUnmapped);
    c.abandon();
}

TEST_F(ManagerTest, the_answer_of_the_client_takes_the_dialog_back)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    const WmWindow* window = make_unresponsive(manager->wm_windows[c.window]);
    manager->check_pings();
    ASSERT_EQ(manager->asked, window);

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
    EXPECT_EQ(manager->asked, nullptr);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsUnmapped);
}

TEST_F(ManagerTest, a_client_that_goes_away_takes_the_dialog_back)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    make_unresponsive(manager->wm_windows[c.window]);
    manager->check_pings();
    ASSERT_NE(manager->asked, nullptr);

    XDestroyWindow(c.display, c.window);
    c.sync();
    run_loop();
    pump_events();

    EXPECT_EQ(manager->asked, nullptr);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsUnmapped);
}

TEST_F(ManagerTest, one_client_is_asked_about_at_a_time)
{
    start();
    FakeClient& c = add_client();
    FakeClient& d = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    d.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    const WmWindow* first = make_unresponsive(manager->wm_windows[c.window]);
    const WmWindow* second = make_unresponsive(manager->wm_windows[d.window]);
    manager->check_pings();
    const WmWindow* shown = manager->asked;
    ASSERT_TRUE(shown == first || shown == second);
    EXPECT_EQ(manager->ping_timeout_ms() >= 0, true);

    manager->check_pings();
    EXPECT_EQ(manager->asked, shown);
    click_dialog_button(manager->wm_unresponsive.get_window(), 2);
    manager->check_pings();

    EXPECT_NE(manager->asked, nullptr); // the other one is asked now
    EXPECT_NE(manager->asked, shown);
}

TEST_F(ManagerTest, the_dialog_belongs_to_the_window_it_asks_about)
{
    start();
    FakeClient& c = add_client();
    const FakeClient& d = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    manager->wm_windows[d.window]->raise(); // over the one that is asked about
    manager->asked = nullptr;
    make_unresponsive(manager->wm_windows[c.window]);

    manager->check_pings();

    ASSERT_EQ(manager->asked, manager->wm_windows[c.window]);
    const auto order = children_of(root_window());
    const auto dialog = std::find(order.begin(), order.end(),
                                  manager->wm_unresponsive.get_window());
    const auto frame =
            std::find(order.begin(), order.end(), parent_of(c.window));
    const auto other =
            std::find(order.begin(), order.end(), parent_of(d.window));
    ASSERT_NE(dialog, order.end());
    ASSERT_NE(frame, order.end());
    ASSERT_NE(other, order.end());
    EXPECT_GT(dialog, frame); // right over its window ...
    EXPECT_LT(dialog, other); // ... not over the others
    const XWindowAttributes f = attributes_of(parent_of(c.window));
    const XWindowAttributes w = attributes_of(manager->wm_unresponsive);
    // centered, but on the screen
    EXPECT_EQ(w.x, std::max(0, f.x + (f.width - w.width) / 2));
    EXPECT_EQ(w.y, std::max(0, f.y + (f.height - w.height) / 2));
}

TEST_F(ManagerTest, a_window_that_answered_meanwhile_is_not_asked_about)
{
    start();
    FakeClient& c = add_client();
    FakeClient& d = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    d.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    WmWindow* first = make_unresponsive(manager->wm_windows[c.window]);
    WmWindow* second = make_unresponsive(manager->wm_windows[d.window]);
    first->postpone_ask(WmWindow::Clock::now() - WmWindow::wait_delay -
                        std::chrono::seconds(5)); // waits the longest
    manager->check_pings();
    ASSERT_EQ(manager->asked, first);

    second->pong(); // it lives again before its turn
    click_dialog_button(manager->wm_unresponsive.get_window(), 2);
    manager->check_pings();

    EXPECT_EQ(manager->asked, nullptr);
    EXPECT_EQ(attributes_of(manager->wm_unresponsive).map_state, IsUnmapped);
}

TEST_F(ManagerTest, a_minimized_window_is_not_asked_about)
{
    start();
    FakeClient& c = add_client();
    FakeClient& d = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    d.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    WmWindow* first = make_unresponsive(manager->wm_windows[c.window]);
    WmWindow* second = make_unresponsive(manager->wm_windows[d.window]);
    first->postpone_ask(WmWindow::Clock::now() - WmWindow::wait_delay -
                        std::chrono::seconds(5));
    manager->check_pings();
    ASSERT_EQ(manager->asked, first);

    second->minimize(); // while the first one is asked about
    click_dialog_button(manager->wm_unresponsive.get_window(), 2);
    manager->check_pings();

    EXPECT_EQ(manager->asked, nullptr);
}

TEST_F(ManagerTest, the_dialog_goes_when_its_window_is_minimized)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    WmWindow* window = make_unresponsive(manager->wm_windows[c.window]);
    manager->check_pings();
    ASSERT_EQ(manager->asked, window);

    window->minimize();

    EXPECT_EQ(manager->asked, nullptr);
}

TEST_F(ManagerTest, a_window_of_a_dead_process_is_not_asked_about)
{
    start();
    FakeClient& c = add_client();
    c.set_protocols({dsp()._NET_WM_PING});
    run_loop();
    pump_events();
    const WmWindow* window = make_unresponsive(manager->wm_windows[c.window]);
    const pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        _exit(0);
    }
    // the manager may reap its children itself, then there is nothing to wait
    // for; either way the process must be gone
    int status = 0;
    for (int i = 0; i < 200 && waitpid(pid, &status, WNOHANG) == 0; ++i) {
        usleep(10000);
    }
    ASSERT_EQ(kill(pid, 0), -1);
    c.set_cardinals(dsp()._NET_WM_PID, {pid});
    std::string host = tiny::wm::local_host_name();
    char* names[] = {host.data()};
    XTextProperty text;
    ASSERT_NE(XStringListToTextProperty(names, 1, &text), 0);
    XSetWMClientMachine(c.display, c.window, &text);
    XFree(text.value);
    c.sync();

    manager->check_pings();

    EXPECT_EQ(manager->asked, nullptr);
    EXPECT_GT(ask_time(*window), WmWindow::Clock::now()); // asked later
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

TEST_F(ManagerTest, destructor_gives_the_clients_back_to_the_root_window)
{
    start();
    FakeClient& framed = add_client();
    FakeClient& bare = add_client();
    bare.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0);
    FakeClient& hidden = add_client();
    run_loop();
    pump_events();
    ASSERT_TRUE(is_framed(framed));
    manager->wm_windows[hidden.window]->minimize();

    manager.reset();

    for (FakeClient* c : {&framed, &bare, &hidden}) {
        EXPECT_EQ(parent_of(c->window), root_window());
        EXPECT_EQ(c->attrs().map_state, IsViewable);
    }
    EXPECT_TRUE(
            root_property<::Window>(dsp()._NET_CLIENT_LIST, XA_WINDOW).empty());
}

TEST_F(ManagerTest, destructor_does_not_leave_a_released_window_minimized)
{
    start();
    FakeClient& c = add_client();
    run_loop();
    pump_events();
    manager->wm_windows[c.window]->minimize();
    ASSERT_TRUE(c.state_has(dsp()._NET_WM_STATE_HIDDEN));

    manager.reset();

    EXPECT_FALSE(c.state_has(dsp()._NET_WM_STATE_HIDDEN));
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

    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), 2u);
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
    EXPECT_EQ(v[1], tiny::wm::shell_theme.panel);
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
    EXPECT_GE(f.y, tiny::wm::shell_theme.panel);
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
    EXPECT_EQ(f.y, 120 - tiny::wm::shell_theme.win_header - f.border_width);
    EXPECT_EQ(f.width, 320);
    EXPECT_EQ(f.height, 210 + tiny::wm::shell_theme.win_header);
    EXPECT_EQ(parent_of(c.window), frame);
    const auto a = c.attrs();
    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, tiny::wm::shell_theme.win_header);
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
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), 1u);
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

TEST_F(ManagerTest, a_client_can_ask_the_manager_to_iconify_its_window)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    const WmWindow* window = manager->wm_windows[c.window];

    request(c.window, dsp().WM_CHANGE_STATE, IconicState);
    run_loop();
    pump_events();

    EXPECT_TRUE(window->get_minimized());
    EXPECT_TRUE(is_managed(c));
    XWindowAttributes frame;
    ASSERT_NE(XGetWindowAttributes(dsp(), parent_of(c.window), &frame), 0);
    EXPECT_EQ(frame.map_state, IsUnmapped); // no empty frame is left
}

TEST_F(ManagerTest, a_request_to_iconify_with_another_state_does_nothing)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();

    request(c.window, dsp().WM_CHANGE_STATE, NormalState);
    request(12345, dsp().WM_CHANGE_STATE, IconicState); // unknown window
    run_loop();
    pump_events();

    EXPECT_FALSE(manager->wm_windows[c.window]->get_minimized());
}

TEST_F(ManagerTest, a_window_that_may_not_be_minimized_ignores_the_request)
{
    start();
    FakeClient& c = add_client();
    c.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                tiny::wm::MotifWMHints::FUNC_MOVE, 0);
    run_loop();
    pump_events();
    ASSERT_TRUE(is_managed(c));

    request(c.window, dsp().WM_CHANGE_STATE, IconicState);
    run_loop();
    pump_events();

    EXPECT_FALSE(manager->wm_windows[c.window]->get_minimized());
}

TEST_F(ManagerTest, unmap_done_by_the_manager_does_not_forget_the_window)
{
    start();
    FakeClient& c = add_client();
    c.set_motif(tiny::wm::MotifWMHints::FLAG_DECORATIONS, 0, 0); // no frame
    run_loop();
    pump_events();
    WmWindow* window = manager->wm_windows[c.window];

    window->minimize(); // unmaps the client itself
    run_loop();
    pump_events();

    EXPECT_TRUE(is_managed(c));
    EXPECT_EQ(c.attrs().map_state, IsUnmapped);
    window->restore();
    run_loop();
    pump_events();
    EXPECT_TRUE(is_managed(c));
    EXPECT_EQ(c.attrs().map_state, IsViewable);
}

TEST_F(ManagerTest, a_minimized_window_withdrawn_by_the_client_is_forgotten)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    pump_events();
    manager->wm_windows[c.window]->minimize();
    run_loop();
    pump_events();
    ASSERT_TRUE(is_managed(c));

    XUnmapWindow(c.display, c.window);
    c.sync();
    run_loop();
    pump_events();

    EXPECT_FALSE(is_managed(c));
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), 1u);
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
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), 1u);
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
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), 1u);
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

    second->on_focus(nullptr, make_event(FocusIn));

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
    window->on_modifier_drag(nullptr, XEvent());
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

// Two applications in a temporary XDG data directory: Epsilon and Eta
class ManagerFavoritesTest : public ManagerTest {
  protected:
    void SetUp() override
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "applications");
        for (const char* name : {"epsilon", "eta"}) {
            std::ofstream(root / "applications" /
                          (std::string(name) + ".desktop"))
                    << "[Desktop Entry]\nName=" << name
                    << "\nType=Application\nExec=touch "
                    << (root / name).string() << "\n";
        }
    }

    void TearDown() override { std::filesystem::remove_all(root); }

    // The icons of the dock, without the dots
    size_t icons()
    {
        return mapped_children_of(manager->wm_dock.get_window()).size() - 1;
    }

    std::filesystem::path root =
            std::filesystem::temp_directory_path() /
            ("tiny-manager-fav-" + std::to_string(getpid()));
    EnvVar data_home{"XDG_DATA_HOME", "/nonexistent"};
    EnvVar data_dirs{"XDG_DATA_DIRS", root.string()};
};

TEST_F(ManagerFavoritesTest, a_favorite_that_does_not_run_has_an_icon)
{
    start();

    manager->set_favorites({"epsilon.desktop", "eta.desktop"});

    EXPECT_EQ(icons(), 2u);
}

TEST_F(ManagerFavoritesTest, a_favorite_that_is_not_installed_has_no_icon)
{
    start();

    manager->set_favorites({"epsilon.desktop", "gone.desktop"});

    EXPECT_EQ(icons(), 1u);
}

TEST_F(ManagerFavoritesTest, a_window_of_a_favorite_takes_its_place_in_the_dock)
{
    FakeClient& client = add_client();
    client.set_class("epsilon", "Epsilon");
    start();
    manager->set_favorites({"epsilon.desktop"});

    EXPECT_EQ(icons(), 1u);
    EXPECT_EQ(manager->wm_windows.at(client.window)->get_app_id(),
              "epsilon.desktop");
}

TEST_F(ManagerFavoritesTest,
       a_window_without_a_desktop_entry_has_no_application)
{
    FakeClient& client = add_client();
    client.set_class("zeta", "Zeta");
    start();

    EXPECT_EQ(manager->wm_windows.at(client.window)->get_app_id(), "");
}

TEST_F(ManagerFavoritesTest, a_click_on_a_favorite_starts_the_application)
{
    start();
    manager->set_favorites({"epsilon.desktop"});

    manager->wm_dock.on_launch("epsilon.desktop");

    for (int i = 0; i < 200 && !std::filesystem::exists(root / "epsilon");
         ++i) {
        usleep(10 * 1000);
    }
    EXPECT_TRUE(std::filesystem::exists(root / "epsilon"));
}

TEST_F(ManagerFavoritesTest, the_menu_adds_and_removes_a_favorite_and_tells_it)
{
    start();
    std::vector<std::vector<std::string>> saved;
    manager->set_favorites({}, [&saved](const std::vector<std::string>& list) {
        saved.push_back(list);
    });

    manager->wm_dock.on_toggle_favorite("epsilon.desktop");
    manager->wm_dock.on_toggle_favorite("eta.desktop");
    EXPECT_EQ(icons(), 2u);
    manager->wm_dock.on_toggle_favorite("epsilon.desktop");

    EXPECT_EQ(icons(), 1u);
    ASSERT_EQ(saved.size(), 3u);
    EXPECT_EQ(saved[0], (std::vector<std::string>{"epsilon.desktop"}));
    EXPECT_EQ(saved[1],
              (std::vector<std::string>{"epsilon.desktop", "eta.desktop"}));
    EXPECT_EQ(saved[2], (std::vector<std::string>{"eta.desktop"}));
}

TEST_F(ManagerFavoritesTest,
       the_menu_of_the_dock_knows_the_actions_of_the_application)
{
    std::ofstream(root / "applications" / "theta.desktop")
            << "[Desktop Entry]\nName=Theta\nType=Application\nExec=true\n"
               "Actions=mark\n[Desktop Action mark]\nName=Mark\nExec=touch "
            << (root / "theta-mark").string() << "\n";
    start();

    EXPECT_EQ(manager->wm_dock.list_actions("theta.desktop"),
              (std::vector<std::string>{"Mark"}));
    EXPECT_TRUE(manager->wm_dock.list_actions("epsilon.desktop").empty());

    manager->wm_dock.on_action("theta.desktop", 0);

    for (int i = 0; i < 200 && !std::filesystem::exists(root / "theta-mark");
         ++i) {
        usleep(10 * 1000);
    }
    EXPECT_TRUE(std::filesystem::exists(root / "theta-mark"));
}

TEST_F(ManagerFavoritesTest, a_favorite_that_is_not_installed_stays_in_the_list)
{
    start();
    std::vector<std::string> saved;
    manager->set_favorites(
            {"gone.desktop"},
            [&saved](const std::vector<std::string>& list) { saved = list; });

    manager->wm_dock.on_toggle_favorite("epsilon.desktop");

    EXPECT_EQ(saved,
              (std::vector<std::string>{"gone.desktop", "epsilon.desktop"}));
}

TEST_F(ManagerFavoritesTest, a_dragged_favorite_changes_the_order_and_tells_it)
{
    start();
    std::vector<std::vector<std::string>> saved;
    manager->set_favorites({"epsilon.desktop", "eta.desktop"},
                           [&saved](const std::vector<std::string>& list) {
                               saved.push_back(list);
                           });

    manager->wm_dock.on_move_favorite("epsilon.desktop", 1);

    ASSERT_EQ(saved.size(), 1u);
    EXPECT_EQ(saved[0],
              (std::vector<std::string>{"eta.desktop", "epsilon.desktop"}));
    EXPECT_EQ(manager->wm_dock.get_favorites().at(0).id, "eta.desktop");
}

TEST_F(ManagerFavoritesTest, a_favorite_that_is_not_installed_keeps_its_place)
{
    start();
    std::vector<std::string> saved;
    manager->set_favorites(
            {"epsilon.desktop", "gone.desktop", "eta.desktop"},
            [&saved](const std::vector<std::string>& list) { saved = list; });

    manager->wm_dock.on_move_favorite("eta.desktop", 0);

    EXPECT_EQ(saved, (std::vector<std::string>{"eta.desktop", "gone.desktop",
                                               "epsilon.desktop"}));
}

TEST_F(ManagerFavoritesTest, the_place_behind_the_last_favorite_is_the_end)
{
    start();
    std::vector<std::string> saved;
    manager->set_favorites(
            {"epsilon.desktop", "eta.desktop"},
            [&saved](const std::vector<std::string>& list) { saved = list; });

    manager->wm_dock.on_move_favorite("epsilon.desktop", 99);

    EXPECT_EQ(saved,
              (std::vector<std::string>{"eta.desktop", "epsilon.desktop"}));
}

TEST_F(ManagerFavoritesTest, an_application_that_is_not_a_favorite_is_not_moved)
{
    start();
    std::vector<std::string> saved;
    manager->set_favorites(
            {"epsilon.desktop"},
            [&saved](const std::vector<std::string>& list) { saved = list; });

    manager->wm_dock.on_move_favorite("eta.desktop", 0);

    EXPECT_TRUE(saved.empty());
}

TEST_F(ManagerFavoritesTest, the_right_button_in_the_launcher_opens_the_menu)
{
    start();
    manager->wm_launcher.popup();

    manager->wm_launcher.on_menu("epsilon.desktop", XButtonEvent{});
    pump_events();

    EXPECT_EQ(attributes_of(manager->wm_dock.get_menu()).map_state, IsViewable);
    // the launcher stays open under the menu
    EXPECT_TRUE(manager->wm_launcher.is_open());
}

TEST_F(ManagerFavoritesTest, the_launcher_gets_the_keyboard_back_after_the_menu)
{
    start();
    manager->wm_launcher.popup();
    manager->wm_launcher.on_menu("epsilon.desktop", XButtonEvent{});
    pump_events();

    manager->wm_dock.get_menu().unmap();

    ::Window focus;
    int revert;
    XGetInputFocus(tiny::get_display(), &focus, &revert);
    EXPECT_NE(focus, PointerRoot);
    EXPECT_TRUE(manager->wm_launcher.is_open());
}

TEST_F(ManagerFavoritesTest,
       starting_an_application_from_the_dock_closes_the_launcher)
{
    start();
    manager->wm_launcher.popup();

    manager->wm_dock.on_launch("epsilon.desktop");

    EXPECT_FALSE(manager->wm_launcher.is_open());
    // the started program must be done before the directory goes away
    for (int i = 0; i < 200 && !std::filesystem::exists(root / "epsilon");
         ++i) {
        usleep(10 * 1000);
    }
}

TEST_F(ManagerFavoritesTest,
       starting_an_action_from_the_dock_closes_the_launcher)
{
    start();
    manager->wm_launcher.popup();

    manager->wm_dock.on_action("epsilon.desktop", 0);

    EXPECT_FALSE(manager->wm_launcher.is_open());
}

TEST_F(ManagerFavoritesTest, changes_are_fine_without_anybody_to_save_them)
{
    start();
    manager->set_favorites({});

    manager->wm_dock.on_toggle_favorite("epsilon.desktop");

    EXPECT_EQ(icons(), 1u);
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
    auto it = manager->wm_tops.begin();
    const WmWindow* first = *it;
    const WmWindow* second = *++it;

    manager->on_key_press(key(XK_Tab)); // no event is processed between them
    EXPECT_EQ(manager->active, second);
    manager->on_key_press(key(XK_Tab));
    EXPECT_EQ(manager->active, first);
    manager->on_key_press(key(XK_Tab));

    EXPECT_EQ(manager->active, second);
    EXPECT_TRUE(second->is_focused());
    EXPECT_FALSE(first->is_focused());
}

TEST_F(ManagerTest, alt_tab_switches_between_the_two_last_windows)
{
    manage_three();
    auto it = manager->wm_tops.begin();
    const WmWindow* first = *it;
    WmWindow* second = *++it;
    manager->focus_window(second);

    manager->on_key_press(key(XK_Tab));

    EXPECT_EQ(manager->active, first);
}

TEST_F(ManagerTest, alt_shift_tab_focuses_the_window_used_the_longest_ago)
{
    manage_three();
    auto it = manager->wm_tops.begin();
    const WmWindow* first = *it;
    manager->focus_window(*++it);
    manager->focus_window(manager->wm_tops.back());
    // the order of use: the third, the second, the first

    manager->on_key_press(key(XK_Tab, Mod1Mask | ShiftMask));

    EXPECT_EQ(manager->active, first);
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

TEST_F(ManagerTest, closing_the_active_window_focuses_the_one_used_before)
{
    manage_three();
    auto it = manager->wm_tops.begin();
    ++it;
    WmWindow* second = *it;
    WmWindow* third = *++it;
    manager->focus_window(third);
    manager->focus_window(second);
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

    EXPECT_EQ(manager->active, third); // not the first, the oldest one
}

TEST_F(ManagerTest, alt_tab_with_one_window_does_nothing)
{
    start();
    const FakeClient& only = add_client();
    run_loop();
    pump_events();
    const WmWindow* window = manager->wm_windows[only.window];
    ASSERT_EQ(manager->active, window);

    manager->on_key_press(key(XK_Tab));
    manager->on_key_press(key(XK_Tab, Mod1Mask | ShiftMask));

    EXPECT_EQ(manager->active, window);
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
    const auto before =
            mapped_children_of(manager->wm_dock.get_window()).size();
    FakeClient& dialog = add_client(false);
    dialog.set_transient_for(parent.window);
    dialog.map();
    run_loop();

    EXPECT_TRUE(is_managed(dialog));
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), before);
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

TEST_F(ManagerTest, alt_tab_reaches_the_modal_dialog_of_a_window_not_its_parent)
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
    const auto before =
            mapped_children_of(manager->wm_dock.get_window()).size();
    FakeClient& toolbar = add_client(false);
    set_window_type(toolbar, dsp()._NET_WM_WINDOW_TYPE_TOOLBAR);
    toolbar.set_transient_for(parent.window);
    toolbar.map();
    run_loop();

    EXPECT_TRUE(
            manager->transients_of(manager->wm_windows[parent.window]).empty());
    // a toolbar is not a dialog, but it has no place in the dock either
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), before);
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

TEST_F(ManagerTest, alt_tab_does_not_list_a_dialog_apart_from_its_parent)
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
    manager->focus_window(manager->wm_windows[dialog.window]);

    const std::vector<WmWindow*> windows = manager->recent_windows();

    ASSERT_EQ(windows.size(), 2u);
    EXPECT_EQ(windows[0],
              manager->wm_windows[parent.window]); // owns the dialog
    EXPECT_EQ(windows[1], manager->wm_windows[other.window]);
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
    EXPECT_EQ(e.top, tiny::wm::shell_theme.win_header + e.bottom);
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
              (std::vector<long>{0, 0, tiny::wm::shell_theme.win_header, 0}));

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
              (std::vector<long>{0, 0, tiny::wm::shell_theme.win_header, 0}));
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
    EXPECT_EQ(attributes_of(window->frame()).height,
              tiny::wm::shell_theme.win_header);
    window->shade(false);
    EXPECT_GT(attributes_of(window->frame()).height,
              tiny::wm::shell_theme.win_header);
}

TEST_F(ManagerTest, a_utility_window_has_no_icon_in_the_dock)
{
    start();
    const auto before =
            mapped_children_of(manager->wm_dock.get_window()).size();
    FakeClient& c = add_client(false);
    set_window_type(c, dsp()._NET_WM_WINDOW_TYPE_UTILITY);
    c.map();
    run_loop();

    EXPECT_TRUE(is_managed(c));
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(), before);
}

TEST_F(ManagerTest, skip_taskbar_removes_the_icon_from_the_dock_and_back)
{
    start();
    const FakeClient& c = add_client();
    run_loop();
    auto* window = manager->wm_windows[c.window];
    const auto with_icon =
            mapped_children_of(manager->wm_dock.get_window()).size();
    ASSERT_TRUE(manager->wm_dock.contains(window));

    window->set_skip_taskbar(true);
    EXPECT_FALSE(manager->wm_dock.contains(window));
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(),
              with_icon - 1);

    window->set_skip_taskbar(false);
    EXPECT_TRUE(manager->wm_dock.contains(window));
    EXPECT_EQ(mapped_children_of(manager->wm_dock.get_window()).size(),
              with_icon);
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
          dsp()._NET_WM_STATE_BELOW, dsp()._NET_WM_STATE_DEMANDS_ATTENTION,
          dsp()._NET_SHOWING_DESKTOP, dsp()._NET_DESKTOP_NAMES}) {
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

TEST_F(ManagerTest, the_loop_sleeps_until_the_next_timer)
{
    start();
    EXPECT_EQ(manager->next_timeout_ms(), -1);
    manager->get_timers().after(std::chrono::seconds(60), [] {});
    const int ms = manager->next_timeout_ms();
    EXPECT_GT(ms, 0);
    EXPECT_LE(ms, 60000);
}

TEST_F(ManagerTest, the_sooner_of_a_ping_and_a_timer_wins)
{
    start();
    manager->get_timers().after(std::chrono::hours(1), [] {});
    EXPECT_LE(manager->next_timeout_ms(), 3600000);
    manager->get_timers().after(std::chrono::milliseconds(0), [] {});
    EXPECT_EQ(manager->next_timeout_ms(), 0);
}

TEST_F(ManagerTest, the_main_loop_runs_the_timers)
{
    start();
    int calls = 0;
    manager->get_timers().after(std::chrono::milliseconds(20), [&] {
        ++calls;
        manager->running = false;
    });

    manager->main_loop(); // sleeps in poll() until the timer

    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(manager->get_timers().empty());
}

TEST_F(ManagerTest, the_terminal_goes_to_the_launcher)
{
    start();
    manager->set_terminal({"foot", "-e"}); // nothing to see, it must not fail
}

TEST_F(ManagerTest, the_commands_of_the_main_menu_can_be_set)
{
    start();
    manager->set_power_commands("my-restart", "");
    EXPECT_EQ(manager->restart_command, "my-restart");
    EXPECT_EQ(manager->shutdown_command, "systemctl poweroff");

    manager->set_power_commands("", "my-shutdown");
    EXPECT_EQ(manager->restart_command, "my-restart");
    EXPECT_EQ(manager->shutdown_command, "my-shutdown");
}

namespace {

//! The lines that a wallpaper program wrote, one for each of its runs
size_t runs_of(const std::filesystem::path& marker)
{
    std::ifstream in(marker);
    size_t lines = 0;
    for (std::string line; std::getline(in, line);) {
        ++lines;
    }
    return lines;
}

XConfigureEvent root_configure(::Window window, int width, int height)
{
    XConfigureEvent e = {};
    e.type = ConfigureNotify;
    e.window = window;
    e.width = width;
    e.height = height;
    return e;
}

} // namespace

TEST_F(ManagerTest,
       the_wallpaper_program_runs_at_once_and_when_the_screen_changes)
{
    start();
    const auto marker = marker_file("tiny-manager-wallpaper-");
    manager->set_wallpaper_command(
            {"sh", "-c", "echo run >> " + marker.string()});
    ASSERT_TRUE(wait_for([&] { return runs_of(marker) == 1; }));

    const int width = manager->screen_width;
    const int height = manager->screen_height;
    ASSERT_GT(width, 0);
    // nothing new: the same size, and a window that is not the root
    manager->on_root_configure(root_configure(root_window(), width, height));
    manager->on_root_configure(
            root_configure(root_window() + 1, width + 1, height));
    // the screen has another size (RandR)
    manager->on_root_configure(
            root_configure(root_window(), width + 10, height + 10));

    EXPECT_TRUE(wait_for([&] { return runs_of(marker) == 2; }));
    EXPECT_EQ(manager->screen_width, width + 10);
    EXPECT_EQ(manager->screen_height, height + 10);
    // the same size again is not a change
    manager->on_root_configure(
            root_configure(root_window(), width + 10, height + 10));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(runs_of(marker), 2u);
    std::filesystem::remove(marker);
}

TEST_F(ManagerTest, the_clock_is_at_the_right_end_of_the_panel)
{
    start();

    manager->set_clock_format("%H:%M");

    ASSERT_NE(manager->wm_clock, nullptr);
    EXPECT_EQ(manager->wm_clock->get_parent_window(),
              manager->wm_panel.get_window());
    EXPECT_EQ(attributes_of(*manager->wm_clock).x +
                      manager->wm_clock->get_width() + 1,
              attributes_of(manager->wm_panel).width);
    EXPECT_EQ(manager->wm_clock->get_text().size(), 5u);
    EXPECT_NE(manager->clock_timer, 0u);
}

TEST_F(ManagerTest, the_clock_is_changed_or_removed_by_the_format)
{
    start();
    manager->set_clock_format("%Y");
    EXPECT_EQ(manager->wm_clock->get_text().size(), 4u);

    manager->set_clock_format("%H:%M");
    EXPECT_EQ(manager->wm_clock->get_text().size(), 5u);

    manager->set_clock_format("");
    EXPECT_EQ(manager->wm_clock, nullptr);
    EXPECT_EQ(manager->clock_timer, 0u);
    EXPECT_TRUE(manager->timers.empty());
}

TEST_F(ManagerTest, the_clock_waits_for_the_next_minute)
{
    start();
    manager->set_clock_format("%H:%M");
    const auto first = manager->clock_timer;

    // the timer is the one of the next minute, it is not due yet
    EXPECT_GT(manager->timers.timeout_ms(), 0);
    EXPECT_LE(manager->timers.timeout_ms(), 60000);
    // when it is due it shows the time and sets the next one
    EXPECT_EQ(manager->timers.run_due(tiny::Timers::Clock::now() +
                                      std::chrono::seconds(61)),
              1);
    EXPECT_NE(manager->clock_timer, first);
    EXPECT_GT(manager->timers.timeout_ms(), 0);
}

TEST_F(ManagerTest, the_root_window_events_arrive_to_the_manager)
{
    start();
    const auto marker = marker_file("tiny-manager-wallpaper-event-");
    manager->set_wallpaper_command(
            {"sh", "-c", "echo run >> " + marker.string()});
    ASSERT_TRUE(wait_for([&] { return runs_of(marker) == 1; }));

    // what the server sends to the root when the screen changes its size
    XEvent e = {};
    e.xconfigure = root_configure(root_window(), manager->screen_width + 4,
                                  manager->screen_height);
    e.xconfigure.event = root_window();
    XSendEvent(tiny::get_display(), root_window(), False, StructureNotifyMask,
               &e);
    run_loop();

    EXPECT_TRUE(wait_for([&] { return runs_of(marker) == 2; }));
    std::filesystem::remove(marker);
}

TEST_F(ManagerTest, no_wallpaper_program_means_a_screen_change_runs_nothing)
{
    start();
    ASSERT_TRUE(manager->wallpaper_command.empty());
    manager->on_root_configure(root_configure(root_window(), 1, 1));
    EXPECT_EQ(manager->screen_width, 1); // it is only remembered
}

TEST_F(ManagerTest,
       a_wallpaper_of_the_style_is_drawn_again_when_the_style_changes)
{
    const VariantGuard guard;
    start();
    const auto marker = marker_file("tiny-manager-wallpaper-style-");
    manager->set_variant(tiny::Variant::Dark);
    manager->set_wallpaper_command(
            {"sh", "-c", "echo \"$@\" >> " + marker.string(), "x"}, true);
    ASSERT_TRUE(wait_for([&] { return runs_of(marker) == 1; }));

    manager->set_variant(tiny::Variant::Light);

    ASSERT_TRUE(wait_for([&] { return runs_of(marker) == 2; }));
    std::ifstream in(marker);
    std::string first;
    std::string second;
    std::getline(in, first);
    std::getline(in, second);
    EXPECT_EQ(first, "--style dark");
    EXPECT_EQ(second, "--style light");
}

TEST_F(ManagerTest,
       the_variant_changes_the_color_of_the_root_but_not_a_wallpaper)
{
    const VariantGuard guard;
    start();
    // a point in the middle: the panel and the dock are over the edges
    auto root_pixel = [this] {
        const int y = manager->screen_height / 2;
        return Image(root_window(), 1, y + 1).at(0, y);
    };

    manager->set_variant(tiny::Variant::Light);
    EXPECT_EQ(root_pixel(), tiny::wm::shell_theme.root_background & 0xFFFFFFu);

    manager->wallpaper_command = {"true"};
    XSetWindowBackground(tiny::get_display(), root_window(), 0x123456);
    XClearWindow(tiny::get_display(), root_window());
    manager->set_variant(tiny::Variant::Dark);
    EXPECT_EQ(root_pixel(), 0x123456u);
}

// ---- the mode of the dock ----

class DockModeTest : public ManagerTest {
  protected:
    // A client that lies over the dock, the active window
    WmWindow* cover_the_dock()
    {
        XWindowAttributes root;
        XGetWindowAttributes(dsp(), root_window(), &root);
        clients.push_back(std::make_unique<FakeClient>(
                0, root.height - 30, static_cast<unsigned>(root.width), 20));
        clients.back()->map();
        start();
        pump_events();
        WmWindow* covering = manager->wm_windows.at(clients.back()->window);
        manager->active = covering;
        manager->update_dock_visibility();
        return covering;
    }

    bool dock_shown()
    {
        return attributes_of(manager->wm_dock).map_state != IsUnmapped;
    }

    bool strip_shown()
    {
        return attributes_of(manager->wm_dock_reveal).map_state != IsUnmapped;
    }

    void pointer_leaves_the_dock(int detail = NotifyAncestor)
    {
        XEvent e = make_event(LeaveNotify);
        e.xcrossing.window = manager->wm_dock.get_window();
        e.xcrossing.detail = detail;
        send(manager->wm_dock.get_window(), e);
    }
};

TEST_F(DockModeTest,
       the_dock_hides_by_default_and_shows_while_nothing_covers_it)
{
    add_client();
    start();
    pump_events();

    EXPECT_EQ(manager->get_dock_mode(), tiny::wm::DockMode::AutoHide);
    EXPECT_TRUE(dock_shown());
    EXPECT_FALSE(strip_shown());
}

TEST_F(DockModeTest, a_window_over_the_dock_hides_it)
{
    cover_the_dock();

    EXPECT_FALSE(dock_shown());
    EXPECT_TRUE(strip_shown());
}

TEST_F(DockModeTest,
       the_strip_stays_over_the_windows_also_after_they_are_raised)
{
    WmWindow* covering = cover_the_dock();
    manager->active = covering;

    manager->restack_layers();

    const std::vector<::Window> order = children_of(root_window());
    const auto strip = std::find(order.begin(), order.end(),
                                 manager->wm_dock_reveal.get_window());
    const auto frame = std::find(order.begin(), order.end(), covering->frame());
    ASSERT_NE(strip, order.end());
    ASSERT_NE(frame, order.end());
    EXPECT_GT(strip, frame);
}

TEST_F(DockModeTest, a_window_that_is_raised_later_does_not_hide_the_strip)
{
    const WmWindow* covering = cover_the_dock();
    ASSERT_TRUE(strip_shown());

    XRaiseWindow(dsp(), covering->frame()); // like a full screen window does
    XSync(dsp(), False);
    run_loop();

    const std::vector<::Window> order = children_of(root_window());
    const auto strip = std::find(order.begin(), order.end(),
                                 manager->wm_dock_reveal.get_window());
    const auto frame = std::find(order.begin(), order.end(), covering->frame());
    EXPECT_GT(strip, frame);
}

TEST_F(DockModeTest, the_strip_is_at_the_bottom_edge_of_the_screen)
{
    cover_the_dock();
    XWindowAttributes root;
    XGetWindowAttributes(dsp(), root_window(), &root);

    const XWindowAttributes strip = attributes_of(manager->wm_dock_reveal);

    const XWindowAttributes dock = attributes_of(manager->wm_dock);
    EXPECT_EQ(strip.x, dock.x); // as wide as the dock
    EXPECT_EQ(strip.width, dock.width);
    EXPECT_EQ(strip.height, tiny::wm::shell_theme.dock_reveal);
    EXPECT_EQ(strip.y + strip.height, root.height);
}

TEST_F(DockModeTest, leaving_by_a_grab_of_a_menu_does_not_hide_the_dock)
{
    cover_the_dock();
    send(manager->wm_dock_reveal, EnterNotify);
    XEvent e = make_event(LeaveNotify);
    e.xcrossing.window = manager->wm_dock.get_window();
    e.xcrossing.detail = NotifyAncestor;
    e.xcrossing.mode = NotifyGrab;

    send(manager->wm_dock.get_window(), e);

    EXPECT_TRUE(dock_shown());
}

TEST_F(DockModeTest, the_pointer_at_the_edge_shows_the_dock)
{
    cover_the_dock();

    send(manager->wm_dock_reveal, EnterNotify);

    EXPECT_TRUE(dock_shown());
    EXPECT_FALSE(strip_shown());
}

TEST_F(DockModeTest, leaving_the_dock_hides_it_again)
{
    cover_the_dock();
    send(manager->wm_dock_reveal, EnterNotify);

    pointer_leaves_the_dock();

    EXPECT_FALSE(dock_shown());
    EXPECT_TRUE(strip_shown());
}

TEST_F(DockModeTest, moving_over_the_icons_does_not_hide_the_dock)
{
    cover_the_dock();
    send(manager->wm_dock_reveal, EnterNotify);

    pointer_leaves_the_dock(NotifyInferior);

    EXPECT_TRUE(dock_shown());
}

TEST_F(DockModeTest, an_open_menu_keeps_the_dock)
{
    cover_the_dock();
    send(manager->wm_dock_reveal, EnterNotify);
    manager->wm_dock.get_menu().set_items({"Close"});
    manager->wm_dock.get_menu().map();

    pointer_leaves_the_dock();

    EXPECT_TRUE(dock_shown());
    manager->wm_dock.get_menu().unmap();
    manager->wm_dock.on_menu_closed();
    EXPECT_FALSE(dock_shown());
}

TEST_F(DockModeTest, a_minimized_window_covers_nothing)
{
    WmWindow* covering = cover_the_dock();
    ASSERT_FALSE(dock_shown());

    covering->minimize();
    manager->update_dock_visibility();

    EXPECT_TRUE(dock_shown());
}

TEST_F(DockModeTest, the_dock_comes_back_when_the_window_goes)
{
    cover_the_dock();
    ASSERT_FALSE(dock_shown());

    const XErrors errors;
    XDestroyWindow(clients.back()->display, clients.back()->window);
    clients.back()->sync();
    run_loop();

    EXPECT_TRUE(dock_shown());
}

TEST_F(DockModeTest, the_mode_on_never_hides_the_dock)
{
    cover_the_dock();

    manager->set_dock_mode(tiny::wm::DockMode::On);

    EXPECT_TRUE(dock_shown());
    EXPECT_FALSE(strip_shown());
}

TEST_F(DockModeTest, the_mode_off_never_shows_the_dock)
{
    add_client();
    start();
    pump_events();

    manager->set_dock_mode(tiny::wm::DockMode::Off);

    EXPECT_FALSE(dock_shown());
    EXPECT_FALSE(strip_shown());
}

TEST_F(DockModeTest, the_dock_is_shown_again_when_the_mode_changes_back)
{
    add_client();
    start();
    manager->set_dock_mode(tiny::wm::DockMode::Off);

    manager->set_dock_mode(tiny::wm::DockMode::AutoHide);

    EXPECT_TRUE(dock_shown());
}

// ---- the window switcher (Alt + Tab with Alt held) ----

class ManagerSwitcherTest : public ManagerFavoritesTest {
  protected:
    // The order of use: the third, the second, the first; Alt is held
    void held()
    {
        manage_three();
        auto it = manager->wm_tops.begin();
        first = *it;
        second = *++it;
        third = *++it;
        manager->focus_window(second);
        manager->focus_window(third);
        manager->is_alt_down = [] { return true; };
    }

    void press(KeySym keysym, unsigned int state = Mod1Mask)
    {
        manager->on_key_press(key(keysym, state));
    }

    void release(KeySym keysym, unsigned int state = 0)
    {
        XKeyEvent e = key(keysym, state);
        e.type = KeyRelease;
        manager->on_key_release(e);
    }

    // Another client tries to take the keyboard: it fails while we have it
    bool keyboard_is_ours()
    {
        const FakeClient other;
        const int status =
                XGrabKeyboard(other.display, root_window(), False,
                              GrabModeAsync, GrabModeAsync, CurrentTime);
        if (status == GrabSuccess) {
            XUngrabKeyboard(other.display, CurrentTime);
        }
        other.sync();
        return status == AlreadyGrabbed;
    }

    WmWindow* first = nullptr;
    WmWindow* second = nullptr;
    WmWindow* third = nullptr;
};

TEST_F(ManagerSwitcherTest, alt_tab_shows_the_switcher_and_switches_nothing)
{
    held();

    press(XK_Tab);

    EXPECT_TRUE(manager->wm_switcher.is_open());
    EXPECT_EQ(manager->active, third);
    EXPECT_TRUE(keyboard_is_ours());
    EXPECT_TRUE(manager->key_done);
}

TEST_F(ManagerSwitcherTest, the_window_is_switched_when_alt_is_released)
{
    held();
    press(XK_Tab);

    release(XK_Alt_L);

    EXPECT_FALSE(manager->wm_switcher.is_open());
    EXPECT_EQ(manager->active, second); // the window used before
    EXPECT_FALSE(keyboard_is_ours());
}

TEST_F(ManagerSwitcherTest, the_right_alt_switches_as_well)
{
    held();
    press(XK_Tab);

    release(XK_Alt_R);

    EXPECT_EQ(manager->active, second);
}

TEST_F(ManagerSwitcherTest, other_keys_do_not_close_the_switcher)
{
    held();
    press(XK_Tab);

    release(XK_Tab, Mod1Mask);
    press(XK_a);
    release(XK_a);

    EXPECT_TRUE(manager->wm_switcher.is_open());
    EXPECT_EQ(manager->active, third);
}

TEST_F(ManagerSwitcherTest, tab_goes_on_and_shift_tab_goes_back)
{
    held();
    press(XK_Tab); // the second, the order: third, second, first

    press(XK_Tab); // the first
    press(XK_Tab, Mod1Mask | ShiftMask);
    release(XK_Alt_L);

    EXPECT_EQ(manager->active, second);
}

TEST_F(ManagerSwitcherTest,
       alt_shift_tab_starts_with_the_window_used_the_longest_ago)
{
    held();

    press(XK_Tab, Mod1Mask | ShiftMask);
    release(XK_Alt_L);

    EXPECT_EQ(manager->active, first);
}

TEST_F(ManagerSwitcherTest, the_right_arrow_goes_on_like_tab)
{
    held();
    press(XK_Tab);

    press(XK_Right);
    release(XK_Alt_L);

    EXPECT_EQ(manager->active, first);
}

TEST_F(ManagerSwitcherTest, the_left_arrow_goes_back_like_shift_tab)
{
    held();
    press(XK_Tab);

    press(XK_Left); // the current window: nothing to switch
    release(XK_Alt_L);

    EXPECT_EQ(manager->active, third);
}

TEST_F(ManagerSwitcherTest, down_and_up_show_and_hide_the_windows)
{
    held();
    press(XK_Tab);

    press(XK_Down);
    EXPECT_EQ(attributes_of(manager->wm_switcher.get_windows_panel()).map_state,
              IsViewable);

    press(XK_Up);
    EXPECT_EQ(attributes_of(manager->wm_switcher.get_windows_panel()).map_state,
              IsUnmapped);
}

TEST_F(ManagerSwitcherTest,
       alt_semicolon_goes_to_the_other_window_of_the_application)
{
    FakeClient& one = add_client();
    one.set_class("epsilon", "Epsilon");
    FakeClient& two = add_client();
    two.set_class("epsilon", "Epsilon");
    const FakeClient& other = add_client();
    start();
    manager->is_alt_down = [] { return true; };
    WmWindow* a = manager->wm_windows.at(one.window);
    WmWindow* b = manager->wm_windows.at(two.window);
    manager->focus_window(b);
    manager->focus_window(manager->wm_windows.at(other.window));
    manager->focus_window(a); // the order: a, other, b

    press(XK_semicolon);
    release(XK_Alt_L);

    EXPECT_EQ(manager->active, b);
}

TEST_F(ManagerSwitcherTest,
       down_chooses_a_window_of_the_application_with_arrows)
{
    FakeClient& one = add_client();
    one.set_class("epsilon", "Epsilon");
    FakeClient& two = add_client();
    two.set_class("epsilon", "Epsilon");
    start();
    manager->is_alt_down = [] { return true; };
    WmWindow* a = manager->wm_windows.at(one.window);
    WmWindow* b = manager->wm_windows.at(two.window);
    manager->focus_window(b);
    manager->focus_window(a); // one application, a and b

    press(XK_Tab); // there is no other application
    press(XK_Down);
    press(XK_Right);
    release(XK_Alt_L);

    EXPECT_EQ(manager->active, b);
}

TEST_F(ManagerSwitcherTest, alt_semicolon_shows_the_windows_of_the_application)
{
    held();

    press(XK_semicolon);

    EXPECT_TRUE(manager->wm_switcher.is_open());
    EXPECT_EQ(attributes_of(manager->wm_switcher.get_windows_panel()).map_state,
              IsViewable);
    press(XK_semicolon, Mod1Mask | ShiftMask);
    release(XK_Alt_L);
    EXPECT_EQ(manager->active, third); // one window per application: no change
}

TEST_F(ManagerSwitcherTest, escape_closes_the_switcher_and_switches_nothing)
{
    held();
    press(XK_Tab);

    press(XK_Escape);
    release(XK_Alt_L);

    EXPECT_FALSE(manager->wm_switcher.is_open());
    EXPECT_EQ(manager->active, third);
    EXPECT_FALSE(keyboard_is_ours());
}

TEST_F(ManagerSwitcherTest, a_quick_tap_switches_at_once)
{
    held();
    manager->is_alt_down = [] { return false; }; // Alt is up before the grab

    press(XK_Tab);

    EXPECT_FALSE(manager->wm_switcher.is_open());
    EXPECT_EQ(manager->active, second);
    EXPECT_FALSE(keyboard_is_ours());
}

TEST_F(ManagerSwitcherTest, without_the_keyboard_it_switches_at_once)
{
    held();
    const FakeClient other;
    XGrabKeyboard(other.display, root_window(), False, GrabModeAsync,
                  GrabModeAsync, CurrentTime);
    other.sync();

    press(XK_Tab);

    EXPECT_FALSE(manager->wm_switcher.is_open());
    EXPECT_EQ(manager->active, second);
}

TEST_F(ManagerSwitcherTest, a_window_that_goes_away_closes_the_switcher)
{
    held();
    press(XK_Tab);
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

    EXPECT_FALSE(manager->wm_switcher.is_open());
    EXPECT_FALSE(keyboard_is_ours());
}

TEST_F(ManagerSwitcherTest, the_release_of_alt_without_the_switcher_is_ignored)
{
    held();

    release(XK_Alt_L);

    EXPECT_EQ(manager->active, third);
}

TEST_F(ManagerSwitcherTest, alt_semicolon_is_a_shortcut_of_the_manager)
{
    start();

    const XErrors errors;
    const FakeClient other;
    XGrabKey(other.display, XKeysymToKeycode(other.display, XK_semicolon),
             Mod1Mask, root_window(), True, GrabModeAsync, GrabModeAsync);
    other.sync();

    EXPECT_EQ(errors.errors(), 1); // BadAccess: already grabbed
}

TEST_F(ManagerSwitcherTest, there_is_nothing_to_switch_without_windows)
{
    start();
    manager->is_alt_down = [] { return true; };

    press(XK_Tab);
    press(XK_semicolon);

    EXPECT_FALSE(manager->wm_switcher.is_open());
    EXPECT_FALSE(keyboard_is_ours());
}

TEST_F(ManagerSwitcherTest, the_application_is_named_by_its_desktop_entry)
{
    FakeClient& client = add_client();
    client.set_class("epsilon", "Epsilon");
    start();

    EXPECT_EQ(manager->switcher_name(*manager->wm_windows.at(client.window)),
              "epsilon");
}

TEST_F(ManagerSwitcherTest, an_application_without_an_entry_is_named_by_class)
{
    FakeClient& client = add_client();
    client.set_class("zeta", "Zeta");
    client.set_title("A title");
    start();

    EXPECT_EQ(manager->switcher_name(*manager->wm_windows.at(client.window)),
              "Zeta");
}

TEST_F(ManagerSwitcherTest, a_window_without_a_class_is_named_by_its_title)
{
    FakeClient& client = add_client();
    client.set_title("A title");
    start();

    EXPECT_EQ(manager->switcher_name(*manager->wm_windows.at(client.window)),
              "A title");
}

TEST_F(ManagerSwitcherTest, a_big_icon_comes_from_the_theme)
{
    std::filesystem::create_directories(root / "pixmaps");
    std::ofstream(root / "pixmaps" / "epsilon.xpm")
            << "/* XPM */\nstatic char * epsilon_xpm[] = {\n"
               "\"2 2 1 1\",\n\"a c #FF0000\",\n\"aa\",\n\"aa\"};\n";
    std::ofstream(root / "applications" / "epsilon.desktop")
            << "[Desktop Entry]\nName=Epsilon\nType=Application\n"
               "Icon=epsilon\nExec=true\n";
    FakeClient& client = add_client();
    client.set_class("epsilon", "Epsilon");
    start();
    const WmWindow& window = *manager->wm_windows.at(client.window);

    const auto big = manager->switcher_icon(
            window, tiny::wm::shell_theme.switcher_window_icon);
    const auto small =
            manager->switcher_icon(window, tiny::wm::shell_theme.icon);

    EXPECT_NE(big.picture, None);
    EXPECT_NE(big.picture, small.picture); // the one of the window is 48 px
    EXPECT_EQ(small.picture, window.get_app_icon().picture);
}

TEST_F(ManagerSwitcherTest, a_window_without_an_application_has_its_own_icon)
{
    FakeClient& client = add_client();
    client.set_icon_pixmap(16, true);
    start();
    const WmWindow& window = *manager->wm_windows.at(client.window);

    const auto cell = manager->switcher_icon(
            window, tiny::wm::shell_theme.switcher_window_icon);

    EXPECT_EQ(cell.picture, None);
    EXPECT_NE(cell.pixmap, None);
    EXPECT_NE(cell.mask, None);
}

TEST_F(ManagerSwitcherTest,
       a_minimized_window_that_is_the_first_is_brought_back)
{
    held();
    third->minimize();

    press(XK_Tab);
    press(XK_Left); // the cursor on the first application, the minimized one
    release(XK_Alt_L);

    EXPECT_FALSE(third->get_minimized());
}

TEST_F(ManagerSwitcherTest, the_actions_bring_back_the_only_minimized_window)
{
    start();
    const FakeClient& only = add_client();
    run_loop();
    pump_events();
    WmWindow* window = manager->wm_windows[only.window];
    window->minimize();

    manager->get_actions().run("next-window");
    EXPECT_FALSE(window->get_minimized());

    window->minimize();
    manager->get_actions().run("prev-window");
    EXPECT_FALSE(window->get_minimized());
}

TEST_F(ManagerSwitcherTest,
       a_failed_grab_does_not_end_the_grab_of_another_client)
{
    held();
    const FakeClient other;
    XGrabKeyboard(other.display, root_window(), False, GrabModeAsync,
                  GrabModeAsync, CurrentTime);
    other.sync();

    press(XK_Tab);

    const FakeClient probe;
    EXPECT_EQ(XGrabKeyboard(probe.display, root_window(), False, GrabModeAsync,
                            GrabModeAsync, CurrentTime),
              AlreadyGrabbed);
}

TEST_F(ManagerSwitcherTest,
       alt_is_left_to_a_key_that_has_semicolon_only_shifted)
{
    start();
    ::Display* dsp = tiny::get_display();
    int lowest = 0;
    int last = 0;
    XDisplayKeycodes(dsp, &lowest, &last);
    const auto spare = static_cast<KeyCode>(last);
    KeySym shifted[] = {XK_a,     XK_semicolon, NoSymbol,
                        NoSymbol, XK_b,         XK_semicolon};
    XChangeKeyboardMapping(dsp, spare, 6, shifted, 1);
    XMappingEvent change = {};
    change.type = MappingNotify;
    change.display = dsp;
    change.request = MappingKeyboard;
    change.first_keycode = spare;
    change.count = 1;

    manager->on_mapping_notify(change);

    // Shift + ; and AltGr + ; type other characters (: and so on)
    EXPECT_FALSE(manager->is_semicolon(spare));
    const XErrors errors;
    const FakeClient other;
    XGrabKey(other.display, spare, Mod1Mask, root_window(), True, GrabModeAsync,
             GrabModeAsync);
    other.sync();
    EXPECT_EQ(errors.errors(), 0);

    KeySym none = NoSymbol;
    XChangeKeyboardMapping(dsp, spare, 1, &none, 1);
}

namespace {

//! The layout puts a keysym to another key until the object is gone
class MovedKey {
  public:
    MovedKey(KeySym keysym, KeyCode target_arg):
        display(tiny::get_display()),
        original((refresh(display), XKeysymToKeycode(display, keysym))),
        target(target_arg)
    {
        int per_key = 0;
        KeySym* map = XGetKeyboardMapping(display, original, 1, &per_key);
        saved.assign(map, map + per_key);
        XFree(map);
        KeySym none = NoSymbol;
        XChangeKeyboardMapping(display, original, 1, &none, 1);
        XChangeKeyboardMapping(display, target, 1, &keysym, 1);
    }

    ~MovedKey()
    {
        KeySym none = NoSymbol;
        XChangeKeyboardMapping(display, target, 1, &none, 1);
        XChangeKeyboardMapping(display, original,
                               static_cast<int>(saved.size()), saved.data(), 1);
        refresh(display);
    }

    MovedKey(const MovedKey&) = delete;
    MovedKey& operator=(const MovedKey&) = delete;

    //! Tell the manager that the keyboard changed
    void notify(tiny::wm::Manager& manager) const
    {
        XMappingEvent change = {};
        change.type = MappingNotify;
        change.display = display;
        change.request = MappingKeyboard;
        change.first_keycode = target;
        change.count = 1;
        manager.on_mapping_notify(change);
        XSync(display, False); // the grabs are made before another client asks
    }

  private:
    //! Xlib keeps the layout it saw; the test changes it without an event
    static void refresh(::Display* display)
    {
        XMappingEvent change = {};
        change.type = MappingNotify;
        change.display = display;
        change.request = MappingKeyboard;
        change.first_keycode = 8;
        change.count = 248;
        XRefreshKeyboardMapping(&change);
    }

    ::Display* display;
    KeyCode original;
    KeyCode target;
    std::vector<KeySym> saved;
};

} // namespace

TEST_F(ManagerSwitcherTest, alt_tab_and_alt_f2_follow_a_change_of_the_layout)
{
    start();
    int lowest = 0;
    int last = 0;
    XDisplayKeycodes(tiny::get_display(), &lowest, &last);
    const auto tab_key = static_cast<KeyCode>(last); // the layout moves them
    const auto f2_key = static_cast<KeyCode>(last - 1);
    const MovedKey tab(XK_Tab, tab_key);
    const MovedKey f2(XK_F2, f2_key);

    tab.notify(*manager);
    f2.notify(*manager);

    for (const KeyCode key : {tab_key, f2_key}) {
        const XErrors errors;
        const FakeClient other;
        XGrabKey(other.display, key, Mod1Mask, root_window(), True,
                 GrabModeAsync, GrabModeAsync);
        other.sync();
        EXPECT_EQ(errors.errors(), 1)
                << "key " << static_cast<int>(key); // the manager
    }
}

TEST_F(ManagerSwitcherTest, alt_f4_of_a_window_follows_a_change_of_the_layout)
{
    const FakeClient& client = add_client();
    start();
    ASSERT_EQ(manager->wm_windows.size(), 1u);
    int lowest = 0;
    int last = 0;
    XDisplayKeycodes(tiny::get_display(), &lowest, &last);
    const auto spare = static_cast<KeyCode>(last);
    const MovedKey f4(XK_F4, spare);

    f4.notify(*manager);

    const XErrors errors;
    const FakeClient other;
    XGrabKey(other.display, spare, Mod1Mask, client.window, True, GrabModeAsync,
             GrabModeAsync);
    other.sync();
    EXPECT_EQ(errors.errors(), 1); // BadAccess: the manager has it
}

TEST_F(ManagerSwitcherTest, alt_semicolon_follows_a_change_of_the_layout)
{
    start();
    ::Display* dsp = tiny::get_display();
    int lowest = 0;
    int last = 0;
    XDisplayKeycodes(dsp, &lowest, &last);
    const auto spare = static_cast<KeyCode>(last); // the layout moves ; here
    KeySym semicolon = XK_semicolon;
    XChangeKeyboardMapping(dsp, spare, 1, &semicolon, 1);
    XMappingEvent change = {};
    change.type = MappingNotify;
    change.display = dsp;
    change.request = MappingKeyboard;
    change.first_keycode = spare;
    change.count = 1;

    manager->on_mapping_notify(change);

    EXPECT_TRUE(manager->is_semicolon(spare));
    const XErrors errors;
    const FakeClient other;
    XGrabKey(other.display, spare, Mod1Mask, root_window(), True, GrabModeAsync,
             GrabModeAsync);
    other.sync();
    EXPECT_EQ(errors.errors(), 1); // BadAccess: the manager has it

    KeySym none = NoSymbol;
    XChangeKeyboardMapping(dsp, spare, 1, &none, 1);
    change.request = MappingPointer; // a mouse does not change the shortcuts
    manager->on_mapping_notify(change);
    manager->on_mapping_notify(change);
}

namespace {

// What a program does to get an icon in the tray
void request_tray_dock(FakeClient& program, ::Window tray, ::Window icon)
{
    XClientMessageEvent e = {};
    e.type = ClientMessage;
    e.window = tray;
    e.message_type = dsp()._NET_SYSTEM_TRAY_OPCODE;
    e.format = 32;
    e.data.l[0] = CurrentTime;
    e.data.l[1] = 0; // REQUEST_DOCK
    e.data.l[2] = static_cast<long>(icon);
    XSendEvent(program.display, tray, False, NoEventMask,
               reinterpret_cast<XEvent*>(&e));
    program.sync();
}

} // namespace

namespace {

//! The theme name that a program reads from the manager of settings
std::string theme_name_of(const tiny::wm::XSettings& owner)
{
    ::Display* display = tiny::get_display();
    const Atom property = XInternAtom(display, "_XSETTINGS_SETTINGS", False);
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long left = 0;
    unsigned char* data = nullptr;
    std::string text;
    if (XGetWindowProperty(display, owner.get_window(), property, 0, 1 << 16,
                           False, property, &type, &format, &count, &left,
                           &data) == Success &&
        data != nullptr) {
        const std::string all(reinterpret_cast<char*>(data), count);
        XFree(data);
        for (const char* name : {"Adwaita-dark", "Adwaita"}) {
            // the longer name first: "Adwaita" is in "Adwaita-dark"
            if (all.find(name) != std::string::npos) {
                text = name;
                break;
            }
        }
    }
    return text;
}

auto theme_settings()
{
    return [](tiny::Variant variant) {
        return std::vector<tiny::wm::XSetting>{tiny::wm::XSetting::string(
                "Net/ThemeName",
                variant == tiny::Variant::Dark ? "Adwaita-dark" : "Adwaita")};
    };
}

} // namespace

TEST_F(ManagerTest, there_are_no_settings_until_they_are_asked_for)
{
    start();

    EXPECT_EQ(manager->get_xsettings(), nullptr);
}

TEST_F(ManagerTest, the_settings_are_published_for_the_programs)
{
    start();
    manager->set_variant(tiny::Variant::Dark);

    manager->set_xsettings(true, theme_settings());

    ASSERT_NE(manager->get_xsettings(), nullptr);
    EXPECT_EQ(theme_name_of(*manager->get_xsettings()), "Adwaita-dark");
}

TEST_F(ManagerTest, a_change_of_the_theme_reaches_the_programs_that_run)
{
    start();
    manager->set_variant(tiny::Variant::Dark);
    manager->set_xsettings(true, theme_settings());
    const uint32_t serial = manager->get_xsettings()->get_serial();

    manager->set_variant(tiny::Variant::Light);

    EXPECT_GT(manager->get_xsettings()->get_serial(), serial);
    EXPECT_EQ(theme_name_of(*manager->get_xsettings()), "Adwaita");
    manager->set_variant(tiny::Variant::Dark);
}

TEST_F(ManagerTest, asking_for_the_settings_again_keeps_the_owner)
{
    start();
    manager->set_xsettings(true, theme_settings());
    const ::Window owner = manager->get_xsettings()->get_window();

    manager->set_xsettings(true, theme_settings());

    EXPECT_EQ(manager->get_xsettings()->get_window(), owner);
}

TEST_F(ManagerTest, the_settings_can_be_turned_off)
{
    start();
    manager->set_xsettings(true, theme_settings());
    ASSERT_NE(manager->get_xsettings(), nullptr);

    manager->set_xsettings(false);

    EXPECT_EQ(manager->get_xsettings(), nullptr);
    manager->set_variant(tiny::Variant::Light); // nothing to publish
    manager->set_variant(tiny::Variant::Dark);
}

TEST_F(ManagerTest, another_manager_of_settings_is_left_alone)
{
    start();
    const FakeClient daemon;
    const Atom selection = XInternAtom(daemon.display, "_XSETTINGS_S0", False);
    XSetSelectionOwner(daemon.display, selection, daemon.window, CurrentTime);
    daemon.sync();

    manager->set_xsettings(true, theme_settings());

    EXPECT_EQ(manager->get_xsettings(), nullptr);
    EXPECT_EQ(XGetSelectionOwner(tiny::get_display(), selection),
              daemon.window);
}

TEST_F(ManagerTest, the_settings_end_when_a_daemon_takes_the_selection)
{
    start();
    manager->set_xsettings(true, theme_settings());
    XSelectionClearEvent lost = {};
    lost.type = SelectionClear;
    lost.window = manager->get_xsettings()->get_window();
    lost.selection = XInternAtom(tiny::get_display(), "_XSETTINGS_S0", False);

    manager->on_selection_clear(lost);

    EXPECT_EQ(manager->get_xsettings(), nullptr);
}

TEST_F(ManagerTest, there_is_no_tray_until_it_is_asked_for)
{
    start();

    EXPECT_EQ(manager->get_tray(), nullptr);
}

TEST_F(ManagerTest, the_tray_takes_the_icons_of_programs_into_the_panel)
{
    start();
    manager->set_clock_format("%H:%M");
    manager->set_system_tray(true);
    ASSERT_NE(manager->get_tray(), nullptr);
    FakeClient& program = add_client(false);

    request_tray_dock(program, manager->get_tray()->get_window(),
                      program.window);
    run_loop();

    ASSERT_EQ(manager->get_tray()->size(), 1u);
    EXPECT_FALSE(is_managed(program)); // an icon is not a window of the desktop
    const tiny::wm::TrayIcon* icon = manager->get_tray()->find(program.window);
    ASSERT_NE(icon, nullptr);
    EXPECT_EQ(parent_of(program.window), icon->get_window());
    // the clock stays at the end of the panel
    EXPECT_LT(attributes_of(*icon).x, attributes_of(*manager->wm_clock).x);
}

TEST_F(ManagerTest, a_destroyed_icon_leaves_the_tray)
{
    start();
    manager->set_system_tray(true);
    FakeClient& program = add_client(false);
    request_tray_dock(program, manager->get_tray()->get_window(),
                      program.window);
    run_loop();
    ASSERT_EQ(manager->get_tray()->size(), 1u);

    XDestroyWindow(program.display, program.window);
    program.sync();
    run_loop();

    EXPECT_EQ(manager->get_tray()->size(), 0u);
}

TEST_F(ManagerTest, the_tray_can_be_turned_off)
{
    start();
    manager->set_system_tray(true);

    manager->set_system_tray(false);

    EXPECT_EQ(manager->get_tray(), nullptr);
    const Atom selection =
            XInternAtom(dsp(), tiny::wm::tray_selection_name(0).c_str(), False);
    XSync(dsp(), False);
    EXPECT_EQ(XGetSelectionOwner(dsp(), selection), None);
}

TEST_F(ManagerTest, another_tray_is_left_alone)
{
    start();
    const FakeClient other;
    const Atom selection =
            XInternAtom(dsp(), tiny::wm::tray_selection_name(0).c_str(), False);
    XSetSelectionOwner(other.display, selection, other.window, CurrentTime);
    other.sync();

    manager->set_system_tray(true);

    EXPECT_EQ(manager->get_tray(), nullptr);
    EXPECT_EQ(XGetSelectionOwner(dsp(), selection), other.window);
}

TEST_F(ManagerTest, the_tray_is_gone_when_another_one_takes_the_selection)
{
    start();
    manager->set_system_tray(true);
    const FakeClient other;
    const Atom selection =
            XInternAtom(dsp(), tiny::wm::tray_selection_name(0).c_str(), False);

    XSetSelectionOwner(other.display, selection, other.window, CurrentTime);
    other.sync();
    run_loop();

    EXPECT_EQ(manager->get_tray(), nullptr);
}

TEST_F(ManagerTest, an_icon_of_the_tray_is_not_managed_when_it_is_mapped)
{
    start();
    manager->set_system_tray(true);
    FakeClient& program = add_client(false);
    request_tray_dock(program, manager->get_tray()->get_window(),
                      program.window);
    run_loop();
    ASSERT_EQ(manager->get_tray()->size(), 1u);

    XMapRequestEvent e = {};
    e.type = MapRequest;
    e.window = program.window;
    manager->on_map_request(e);

    EXPECT_FALSE(is_managed(program));
}

TEST_F(ManagerTest, a_managed_window_is_not_taken_by_the_tray)
{
    start();
    manager->set_system_tray(true);
    FakeClient& program = add_client();
    run_loop();
    ASSERT_TRUE(is_managed(program));

    request_tray_dock(program, manager->get_tray()->get_window(),
                      program.window);
    run_loop();

    EXPECT_EQ(manager->get_tray()->size(), 0u);
    EXPECT_TRUE(is_managed(program));
}

// ---- several monitors ----

namespace {

// The screen as two monitors side by side, the right one is the primary.
class TwoMonitorsTest : public ManagerTest {
  protected:
    void SetUp() override
    {
        const XWindowAttributes root = attributes_of(root_window());
        half = root.width / 2;
        height = root.height;
    }

    void split(size_t primary = 1)
    {
        start();
        manager->monitors = {{"A", {0, 0, half, height}, false},
                             {"B", {half, 0, 2 * half, height}, false}};
        manager->monitors[1].area.width =
                attributes_of(root_window()).width - half;
        manager->choice = {primary, primary == 1 ? 0u : 1u};
        manager->work_areas.clear();
        manager->arrange_shell();
        XSync(dsp(), False);
    }

    int half = 0;
    int height = 0;
};

} // namespace

TEST_F(TwoMonitorsTest, the_panel_is_on_the_primary_monitor)
{
    split();

    const XWindowAttributes panel = attributes_of(manager->wm_panel);
    EXPECT_EQ(panel.x, half);
    EXPECT_EQ(panel.y, 0);
    EXPECT_EQ(panel.width, attributes_of(root_window()).width - half);
}

TEST_F(TwoMonitorsTest, the_panel_follows_another_primary_monitor)
{
    split();
    manager->choice = {0, 1};
    manager->arrange_shell();
    XSync(dsp(), False);

    const XWindowAttributes panel = attributes_of(manager->wm_panel);
    EXPECT_EQ(panel.x, 0);
    EXPECT_EQ(panel.width, half);
}

TEST_F(TwoMonitorsTest, the_dock_is_centered_on_the_primary_monitor)
{
    split();

    const XWindowAttributes dock = attributes_of(manager->wm_dock);
    const int primary_width = attributes_of(root_window()).width - half;
    EXPECT_EQ(dock.x, half + primary_width / 2 - dock.width / 2);
    EXPECT_EQ(dock.y, height - manager->wm_dock.get_height() - border());
}

TEST_F(TwoMonitorsTest, only_the_primary_monitor_loses_the_panel)
{
    split();

    ASSERT_EQ(manager->work_areas.size(), 2u);
    EXPECT_EQ(manager->work_areas[0].y, 0);
    EXPECT_EQ(manager->work_areas[0].height, height);
    EXPECT_EQ(manager->work_areas[1].y, tiny::wm::shell_theme.panel);
    EXPECT_EQ(manager->work_area.x, half); // _NET_WORKAREA is the primary one
}

TEST_F(TwoMonitorsTest, the_panel_strut_is_published_for_its_monitor)
{
    split();

    Atom type;
    int format;
    unsigned long items;
    unsigned long after;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dsp(), manager->wm_panel.get_window(),
                                 dsp()._NET_WM_STRUT_PARTIAL, 0, 12, False,
                                 XA_CARDINAL, &type, &format, &items, &after,
                                 &data),
              Success);
    ASSERT_EQ(items, 12u);
    const auto* v = reinterpret_cast<const long*>(data);
    EXPECT_EQ(v[2], tiny::wm::shell_theme.panel);
    EXPECT_EQ(v[8], half);
    EXPECT_EQ(v[9], attributes_of(root_window()).width - 1);
    XFree(data);
}

TEST_F(TwoMonitorsTest, a_window_is_maximized_on_its_own_monitor)
{
    split();
    FakeClient& c = add_client(false);
    XMoveWindow(c.display, c.window, 50, 80); // on the left monitor
    c.map();
    run_loop();
    WmWindow* window = manager->wm_windows[c.window];

    window->maximize();
    XSync(dsp(), False);

    const XWindowAttributes f = attributes_of(window->frame());
    EXPECT_EQ(f.x, 0);
    EXPECT_EQ(f.y, 0);
    EXPECT_EQ(f.width, half);
    EXPECT_EQ(f.height, height);
}

TEST_F(TwoMonitorsTest,
       a_window_on_the_primary_monitor_is_maximized_below_the_panel)
{
    split();
    FakeClient& c = add_client(false);
    XMoveWindow(c.display, c.window, half + 50, 80);
    c.map();
    run_loop();
    WmWindow* window = manager->wm_windows[c.window];

    window->maximize();
    XSync(dsp(), False);

    const XWindowAttributes f = attributes_of(window->frame());
    EXPECT_EQ(f.x, half);
    EXPECT_EQ(f.y, tiny::wm::shell_theme.panel);
}

TEST_F(TwoMonitorsTest, a_fullscreen_window_covers_only_its_monitor)
{
    split();
    FakeClient& c = add_client(false);
    XMoveWindow(c.display, c.window, 50, 80);
    c.map();
    run_loop();
    WmWindow* window = manager->wm_windows[c.window];

    window->fullscreen(true);
    XSync(dsp(), False);

    const XWindowAttributes f = attributes_of(window->frame());
    EXPECT_EQ(f.x, 0);
    EXPECT_EQ(f.y, 0);
    EXPECT_EQ(f.width, half);
    EXPECT_EQ(f.height, height);
}

TEST_F(TwoMonitorsTest, a_window_without_a_place_opens_where_the_pointer_is)
{
    split();
    XWarpPointer(dsp(), None, root_window(), 0, 0, 0, 0, 100, 100); // left
    XSync(dsp(), False);
    FakeClient& c = add_client(false);
    XMoveWindow(c.display, c.window, 0, 0);
    c.map();

    run_loop();

    const XWindowAttributes f = attributes_of(parent_of(c.window));
    EXPECT_LT(f.x, half);
    EXPECT_EQ(manager->pointer_monitor(), 0u);
}

TEST_F(TwoMonitorsTest,
       the_run_dialog_opens_in_the_middle_of_the_primary_monitor)
{
    split();

    manager->wm_run.popup();
    XSync(dsp(), False);

    const XWindowAttributes run = attributes_of(manager->wm_run);
    const int primary_width = attributes_of(root_window()).width - half;
    EXPECT_EQ(run.x, half + (primary_width - run.width) / 2);
}

TEST_F(TwoMonitorsTest, a_dialog_is_centered_over_its_parent_on_its_monitor)
{
    split();
    const tiny::Rect left = manager->work_areas[0];

    EXPECT_EQ(left.x, 0);
    EXPECT_EQ(manager->areas_at({10, 10, 100, 100}).work.width, half);
    EXPECT_EQ(manager->areas_at({half + 10, 10, 100, 100}).screen.x, half);
}

TEST_F(TwoMonitorsTest, the_settings_choose_the_primary_monitor)
{
    start();

    manager->set_monitors("A", "auto", tiny::wm::Placement::Keep);
    // the server has one monitor, so the list is what the server says
    EXPECT_EQ(manager->get_monitors().size(), 1u);
    EXPECT_EQ(manager->get_primary_monitor(), 0u);
}

TEST_F(ManagerTest, one_monitor_is_the_whole_screen)
{
    start();

    ASSERT_GE(manager->get_monitors().size(), 1u);
    const XWindowAttributes root = attributes_of(root_window());
    const XWindowAttributes panel = attributes_of(manager->wm_panel);
    EXPECT_EQ(panel.x, manager->get_monitors()[0].area.x);
    EXPECT_EQ(panel.width, manager->get_monitors()[0].area.width);
    EXPECT_LE(panel.width, root.width);
}

TEST_F(TwoMonitorsTest, the_numbers_follow_a_change_of_the_monitors)
{
    split();
    manager->show_monitor_numbers();

    manager->monitors.pop_back();
    manager->choice = {0, std::nullopt};
    manager->arrange_shell();

    EXPECT_EQ(manager->get_monitor_labels().get_labels().size(), 1u);
}

#ifdef TINY_RANDR
TEST_F(ManagerTest, the_monitors_of_the_server_are_used_when_they_change)
{
    start();
    ::Display* display = tiny::get_display();
    const XWindowAttributes screen = attributes_of(root_window());
    const int half = screen.width / 2;
    const char* names[] = {"TinyTestA", "TinyTestB"};
    for (int i = 0; i < 2; ++i) {
        XRRMonitorInfo info = {};
        info.name = XInternAtom(display, names[i], False);
        info.x = i * half;
        info.width = half;
        info.height = screen.height;
        info.mwidth = 100;
        info.mheight = 100;
        XRRSetMonitor(display, root_window(), &info);
    }
    XSync(display, False);

    manager->set_monitors("TinyTestB", "auto", tiny::wm::Placement::Keep);

    EXPECT_EQ(manager->get_monitors().size(), 2u);
    EXPECT_EQ(manager->get_primary_monitor(), 1u);
    EXPECT_EQ(attributes_of(manager->wm_panel).x, half);
    EXPECT_EQ(attributes_of(manager->wm_panel).width, half);

    // the same monitors: nothing is arranged again
    manager->work_areas.clear();
    manager->update_monitors();
    EXPECT_TRUE(manager->work_areas.empty());

    for (const char* name : names) {
        XRRDeleteMonitor(display, root_window(),
                         XInternAtom(display, name, False));
    }
    XSync(display, False);
    manager->update_monitors();
    EXPECT_EQ(manager->get_monitors().size(), 1u);
    EXPECT_EQ(attributes_of(manager->wm_panel).x, 0);
}
#endif

namespace {

void send_monitors_message(long what, long window = 0)
{
    XEvent e = {};
    e.xclient.type = ClientMessage;
    e.xclient.window = root_window();
    e.xclient.message_type = dsp()._TINY_SHELL_MONITORS;
    e.xclient.format = 32;
    e.xclient.data.l[0] = what;
    e.xclient.data.l[1] = window;
    XSendEvent(dsp(), root_window(), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XSync(dsp(), False);
}

} // namespace

TEST_F(TwoMonitorsTest, the_settings_show_the_numbers_while_they_live)
{
    split();
    const ::Window owner =
            XCreateSimpleWindow(dsp(), root_window(), 0, 0, 10, 10, 0, 0, 0);

    send_monitors_message(1, static_cast<long>(owner));
    run_loop();
    EXPECT_TRUE(manager->get_monitor_labels().is_shown());

    XDestroyWindow(dsp(), owner); // the settings were closed
    XSync(dsp(), False);
    run_loop();
    EXPECT_FALSE(manager->get_monitor_labels().is_shown());
}

TEST_F(TwoMonitorsTest, the_settings_hide_the_numbers)
{
    split();
    const ::Window owner =
            XCreateSimpleWindow(dsp(), root_window(), 0, 0, 10, 10, 0, 0, 0);
    send_monitors_message(1, static_cast<long>(owner));
    run_loop();

    send_monitors_message(0);
    run_loop();

    EXPECT_FALSE(manager->get_monitor_labels().is_shown());
    XDestroyWindow(dsp(), owner);
}

TEST_F(TwoMonitorsTest, the_settings_ask_for_the_monitors_to_be_read_again)
{
    split();
    int asked = 0;
    manager->set_reload([&asked] { ++asked; });

    send_monitors_message(2);
    run_loop();

    EXPECT_EQ(asked, 1);
}

TEST_F(TwoMonitorsTest, the_numbers_are_raised_over_a_window_that_comes_up)
{
    split();
    manager->show_monitor_numbers();
    FakeClient& c = add_client(false);
    c.map();
    run_loop();
    run_loop(); // the numbers are raised after the event that restacked

    ::Window root;
    ::Window parent;
    ::Window* kids = nullptr;
    unsigned int count = 0;
    XQueryTree(dsp(), root_window(), &root, &parent, &kids, &count);
    ASSERT_GT(count, 0u);
    std::vector<::Window> order(kids, kids + count);
    XFree(kids);
    const ::Window label =
            manager->get_monitor_labels().get_labels()[0]->get_window();
    const ::Window reveal = manager->wm_dock_reveal.get_window();
    // only the other labels and the strip of the dock may be above the label
    std::vector<::Window> allowed = {reveal};
    for (const auto& other : manager->get_monitor_labels().get_labels()) {
        allowed.push_back(other->get_window());
    }
    const auto at = std::find(order.begin(), order.end(), label);
    ASSERT_NE(at, order.end());
    for (auto it = at + 1; it != order.end(); ++it) {
        EXPECT_NE(std::find(allowed.begin(), allowed.end(), *it),
                  allowed.end());
    }
}

TEST_F(TwoMonitorsTest, the_panel_does_not_reach_the_next_monitor)
{
    split(0);

    const XWindowAttributes panel = attributes_of(manager->wm_panel);
    EXPECT_EQ(panel.border_width, 0);
    EXPECT_LE(panel.x + panel.width, half);
}
