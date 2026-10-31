#include "fake_client.h"
#include "helpers.h"

#include <X11/keysym.h>

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

TEST_F(ManagerTest, set_supported_publishes_the_supported_properties)
{
    start();

    manager->set_supported();
    XSync(dsp(), False);

    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dsp(), root_window(), dsp()._NET_SUPPORTED, 0,
                                 64, False, XA_ATOM, &type, &format, &count,
                                 &left, &data),
              Success);
    ASSERT_EQ(count, 4u);
    EXPECT_EQ((reinterpret_cast<Atom*>(data))[0], dsp()._NET_SUPPORTED);
    EXPECT_EQ((reinterpret_cast<Atom*>(data))[3], dsp()._NET_WM_WINDOW_TYPE);
    XFree(data);
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

TEST_F(ManagerTest, super_opens_the_launcher)
{
    const EnvVar data_home("XDG_DATA_HOME", "/nonexistent");
    const EnvVar data_dirs("XDG_DATA_DIRS", "/nonexistent");
    start();
    ASSERT_NE(XKeysymToKeycode(dsp(), XK_Super_L), 0);

    manager->on_key_press(key(XK_Super_L));
    XSync(dsp(), False);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsViewable);
    EXPECT_TRUE(manager->key_done);

    manager->on_key_press(key(XK_Super_R)); // the second Super closes it
    XSync(dsp(), False);
    EXPECT_EQ(attributes_of(manager->wm_launcher).map_state, IsUnmapped);
}

TEST_F(ManagerTest, other_keys_are_not_handled_on_press)
{
    start();
    manager->key_done = true;

    manager->on_key_press(key(XK_Tab));

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
    manager->key_done = false;
    auto* second = *std::next(manager->wm_tops.begin());

    XKeyEvent e = key(XK_Tab);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_TRUE(second->is_focused());
}

TEST_F(ManagerTest, alt_tab_wraps_around)
{
    manage_three();
    manager->key_done = false;
    manager->active = manager->wm_tops.back();

    XKeyEvent e = key(XK_Tab);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_TRUE(manager->wm_tops.front()->is_focused());
}

TEST_F(ManagerTest, alt_shift_tab_focuses_the_previous_window)
{
    manage_three();
    manager->key_done = false;
    manager->active = manager->wm_tops.back();
    auto* previous = *std::next(manager->wm_tops.rbegin());

    XKeyEvent e = key(XK_Tab, Mod1Mask | ShiftMask);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_TRUE(previous->is_focused());
}

TEST_F(ManagerTest, alt_shift_tab_wraps_around)
{
    manage_three();
    manager->key_done = false;

    XKeyEvent e = key(XK_Tab, Mod1Mask | ShiftMask);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_TRUE(manager->wm_tops.back()->is_focused());
}

TEST_F(ManagerTest, alt_tab_restores_a_minimized_window)
{
    manage_three();
    manager->key_done = false;
    auto* second = *std::next(manager->wm_tops.begin());
    second->minimize();
    ASSERT_TRUE(second->get_minimized());

    XKeyEvent e = key(XK_Tab);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_FALSE(second->get_minimized());
    EXPECT_TRUE(second->is_focused());
}

TEST_F(ManagerTest, alt_shift_tab_restores_a_minimized_window)
{
    manage_three();
    manager->key_done = false;
    auto* last = manager->wm_tops.back();
    last->minimize();

    XKeyEvent e = key(XK_Tab, Mod1Mask | ShiftMask);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_FALSE(last->get_minimized());
}

TEST_F(ManagerTest, alt_tab_without_an_active_window_starts_with_the_first)
{
    manage_three();
    manager->key_done = false;
    manager->active = nullptr;

    XKeyEvent e = key(XK_Tab);
    e.type = KeyRelease;
    manager->on_key_release(e);

    EXPECT_TRUE(manager->wm_tops.front()->is_focused());
}

TEST_F(ManagerTest, alt_tab_without_windows_does_nothing)
{
    start();
    manager->key_done = false;

    XKeyEvent e = key(XK_Tab);
    e.type = KeyRelease;
    manager->on_key_release(e);
    e.state = Mod1Mask | ShiftMask;
    manager->on_key_release(e);
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

TEST_F(ManagerTest, x_errors_are_logged_and_ignored)
{
    start();
    XErrorEvent error = {};
    error.type = 0;
    error.error_code = BadWindow;
    error.display = dsp();

    EXPECT_EQ(tiny::wm::Manager::on_error(dsp(), &error), 0);
}

} // namespace
