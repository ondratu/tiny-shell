#include "client.h"
#include "helpers.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <memory>
#include <string>

#include "wm_window.h"

using wm::MotifWMHints;
using WmWindow = wm::Window;

namespace {

class WmWindowTest : public ::testing::Test {
  protected:
    Client client;

    // Screen size, used by maximize.
    XWindowAttributes screen() const { return attributes_of(root_window()); }

    std::unique_ptr<WmWindow> manage(unsigned long functions = 0xFFFF)
    {
        return std::make_unique<WmWindow>(client.window, root_window(),
                                          functions);
    }
};

XEvent client_message(::Window window, Atom type, long l0 = 0, long l1 = 0,
                      long l2 = 0)
{
    XEvent e = make_event(ClientMessage);
    e.xclient.window = window;
    e.xclient.message_type = type;
    e.xclient.format = 32;
    e.xclient.data.l[0] = l0;
    e.xclient.data.l[1] = l1;
    e.xclient.data.l[2] = l2;
    return e;
}

XEvent property_event(::Window window, Atom atom)
{
    XEvent e = make_event(PropertyNotify);
    e.xproperty.window = window;
    e.xproperty.atom = atom;
    return e;
}

} // namespace

// ---- static helpers ----

TEST_F(WmWindowTest, motif_hints_default_to_everything_allowed)
{
    unsigned long functions = 0, decorations = 0;

    EXPECT_FALSE(
            WmWindow::get_motif_hints(client.window, functions, decorations));

    EXPECT_TRUE(functions & MotifWMHints::FUNC_RESIZE);
    EXPECT_TRUE(functions & MotifWMHints::FUNC_CLOSE);
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_TITLE);
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_BORDER);
}

TEST_F(WmWindowTest, motif_hints_without_decorations)
{
    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0, 0);
    unsigned long functions = 0, decorations = 1;

    EXPECT_TRUE(
            WmWindow::get_motif_hints(client.window, functions, decorations));

    EXPECT_EQ(decorations, 0u);
    EXPECT_TRUE(functions & MotifWMHints::FUNC_MOVE); // not overridden
}

TEST_F(WmWindowTest, motif_hints_override_functions)
{
    client.set_motif(MotifWMHints::FLAG_FUNCTIONS, MotifWMHints::FUNC_CLOSE, 0);
    unsigned long functions = 0, decorations = 0;

    EXPECT_TRUE(
            WmWindow::get_motif_hints(client.window, functions, decorations));

    EXPECT_EQ(functions, static_cast<unsigned long>(MotifWMHints::FUNC_CLOSE));
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_TITLE); // not overridden
}

TEST_F(WmWindowTest, net_wm_type_defaults_to_normal)
{
    EXPECT_EQ(WmWindow::get_net_wm_type(client.window),
              WmWindow::WMType::NORMAL);
}

TEST_F(WmWindowTest, net_wm_type_is_read_from_the_property)
{
    struct {
        Atom atom;
        WmWindow::WMType type;
    } cases[] = {
            {dsp()._NET_WM_WINDOW_TYPE_DESKTOP, WmWindow::WMType::DESKTOP},
            {dsp()._NET_WM_WINDOW_TYPE_DOCK, WmWindow::WMType::DOCK},
            {dsp()._NET_WM_WINDOW_TYPE_TOOLBAR, WmWindow::WMType::TOOLBAR},
            {dsp()._NET_WM_WINDOW_TYPE_MENU, WmWindow::WMType::MENU},
            {dsp()._NET_WM_WINDOW_TYPE_UTILITY, WmWindow::WMType::UTILITY},
            {dsp()._NET_WM_WINDOW_TYPE_SPLASH, WmWindow::WMType::SPLASH},
            {dsp()._NET_WM_WINDOW_TYPE_DIALOG, WmWindow::WMType::DIALOG},
            {dsp()._NET_WM_WINDOW_TYPE_NORMAL, WmWindow::WMType::NORMAL},
    };

    for (auto& c : cases) {
        client.set_atoms(dsp()._NET_WM_WINDOW_TYPE, {c.atom});
        EXPECT_EQ(WmWindow::get_net_wm_type(client.window), c.type);
    }
}

TEST_F(WmWindowTest, net_wm_type_uses_the_first_known_type)
{
    client.set_atoms(dsp()._NET_WM_WINDOW_TYPE,
                     {dsp().WM_NAME, dsp()._NET_WM_WINDOW_TYPE_DIALOG,
                      dsp()._NET_WM_WINDOW_TYPE_DOCK});

    EXPECT_EQ(WmWindow::get_net_wm_type(client.window),
              WmWindow::WMType::DIALOG);
}

TEST_F(WmWindowTest, get_properties_lists_window_properties)
{
    client.set_title("title");
    client.set_state({});
    std::set<Atom> properties;

    int count = WmWindow::get_properties(client.window, properties);

    EXPECT_EQ(count, static_cast<int>(properties.size()));
    EXPECT_TRUE(properties.count(dsp().WM_NAME));
    EXPECT_TRUE(properties.count(dsp()._NET_WM_STATE));
}

TEST_F(WmWindowTest, get_properties_of_window_without_properties)
{
    std::set<Atom> properties = {dsp().WM_NAME};

    EXPECT_EQ(WmWindow::get_properties(client.window, properties), 0);

    EXPECT_TRUE(properties.empty());
}

// ---- construction ----

TEST_F(WmWindowTest, plain_window_has_no_special_state)
{
    auto window = manage();

    EXPECT_EQ(window->get_child(), client.window);
    EXPECT_FALSE(window->get_minimized());
    EXPECT_FALSE(window->get_maximized());
    EXPECT_FALSE(window->is_fullscreen());
    EXPECT_FALSE(window->is_focused());
    EXPECT_FALSE(window->skip_taskbar());
    EXPECT_FALSE(window->skip_pager());
    EXPECT_EQ(window->get_wm_name(), "");
    EXPECT_EQ(window->get_icon(), 0u);
}

TEST_F(WmWindowTest, resize_and_maximize_depend_on_functions)
{
    auto window = manage(MotifWMHints::FUNC_RESIZE);
    EXPECT_TRUE(window->is_resizable());
    EXPECT_FALSE(window->is_maximizable());

    window = manage(MotifWMHints::FUNC_MAXIMIZE);
    EXPECT_FALSE(window->is_resizable());
    EXPECT_TRUE(window->is_maximizable());
}

TEST_F(WmWindowTest, initial_wm_states_are_read)
{
    client.set_state({dsp()._NET_WM_STATE_SKIP_TASKBAR,
                      dsp()._NET_WM_STATE_SKIP_PAGER,
                      dsp()._NET_WM_STATE_FULLSCREEN,
                      dsp()._NET_WM_STATE_HIDDEN, dsp()._NET_WM_STATE_FOCUSED});

    auto window = manage();

    EXPECT_TRUE(window->skip_taskbar());
    EXPECT_TRUE(window->skip_pager());
    EXPECT_TRUE(window->is_fullscreen());
    EXPECT_TRUE(window->get_minimized());
    EXPECT_TRUE(window->is_focused());
    EXPECT_FALSE(window->get_maximized());
}

TEST_F(WmWindowTest, all_wm_states_round_trip_through_the_property)
{
    std::vector<Atom> all = {dsp()._NET_WM_STATE_MODAL,
                             dsp()._NET_WM_STATE_STICKY,
                             dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                             dsp()._NET_WM_STATE_MAXIMIZED_HORZ,
                             dsp()._NET_WM_STATE_SHADED,
                             dsp()._NET_WM_STATE_SKIP_TASKBAR,
                             dsp()._NET_WM_STATE_SKIP_PAGER,
                             dsp()._NET_WM_STATE_HIDDEN,
                             dsp()._NET_WM_STATE_FULLSCREEN,
                             dsp()._NET_WM_STATE_ABOVE,
                             dsp()._NET_WM_STATE_BELOW,
                             dsp()._NET_WM_STATE_DEMANDS_ATTENTION,
                             dsp()._NET_WM_STATE_FOCUSED};
    client.set_state(all);
    auto window = manage();

    std::vector<Atom> atoms;
    window->get_wm_states(atoms);

    EXPECT_EQ(atoms, all);
}

TEST_F(WmWindowTest, unknown_wm_state_is_ignored)
{
    client.set_state({dsp().WM_NAME, dsp()._NET_WM_STATE_STICKY});

    auto window = manage();
    std::vector<Atom> atoms;
    window->get_wm_states(atoms);

    EXPECT_EQ(atoms, std::vector<Atom>{dsp()._NET_WM_STATE_STICKY});
}

TEST_F(WmWindowTest, empty_wm_state_property_clears_the_state)
{
    client.set_state({dsp()._NET_WM_STATE_HIDDEN});
    auto window = manage();
    ASSERT_TRUE(window->get_minimized());
    client.set_state({});

    window->update_wm_states();

    EXPECT_FALSE(window->get_minimized());
}

TEST_F(WmWindowTest, title_is_read_from_wm_name)
{
    client.set_title("old style");

    EXPECT_EQ(manage()->get_wm_name(), "old style");
}

TEST_F(WmWindowTest, net_wm_name_wins_over_wm_name)
{
    client.set_title("old style");
    client.set_utf8(dsp()._NET_WM_NAME, "ěšč");

    EXPECT_EQ(manage()->get_wm_name(), "ěšč");
}

TEST_F(WmWindowTest, normal_hints_without_limits_disable_resizing)
{
    client.set_size_hints(PPosition, 0, 0, 0, 0);

    auto window =
            manage(MotifWMHints::FUNC_RESIZE | MotifWMHints::FUNC_MAXIMIZE);

    EXPECT_FALSE(window->is_resizable());
    EXPECT_FALSE(window->is_maximizable());
}

TEST_F(WmWindowTest, normal_hints_with_equal_limits_disable_resizing)
{
    client.set_size_hints(PMinSize | PMaxSize, 300, 200, 300, 200);

    auto window =
            manage(MotifWMHints::FUNC_RESIZE | MotifWMHints::FUNC_MAXIMIZE);

    EXPECT_FALSE(window->is_resizable());
    EXPECT_FALSE(window->is_maximizable());
}

TEST_F(WmWindowTest, normal_hints_with_range_keep_resizing)
{
    client.set_size_hints(PMinSize | PMaxSize, 100, 100, 800, 600);

    auto window =
            manage(MotifWMHints::FUNC_RESIZE | MotifWMHints::FUNC_MAXIMIZE);

    EXPECT_TRUE(window->is_resizable());
    EXPECT_TRUE(window->is_maximizable());
}

TEST_F(WmWindowTest, window_manager_protocols_are_read)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();

    window->close();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_DELETE_WINDOW));
}

// ---- icons ----

TEST_F(WmWindowTest, wm_hints_icon_of_the_theme_size_is_copied)
{
    client.set_icon_pixmap(tiny::theme.wm_icon, false);

    auto window = manage();

    ASSERT_NE(window->get_icon(), 0u);
    EXPECT_EQ(window->get_icon_mask(), 0u);
}

TEST_F(WmWindowTest, wm_hints_icon_of_the_theme_size_with_mask)
{
    client.set_icon_pixmap(tiny::theme.wm_icon, true);

    auto window = manage();

    EXPECT_NE(window->get_icon(), 0u);
    EXPECT_NE(window->get_icon_mask(), 0u);
}

TEST_F(WmWindowTest, wm_hints_icon_of_other_size_is_scaled)
{
    client.set_icon_pixmap(16, true);

    auto window = manage();

    ASSERT_NE(window->get_icon(), 0u);
    ASSERT_NE(window->get_icon_mask(), 0u);
    ::Window root;
    int x, y;
    unsigned int w, h, border, depth;
    XGetGeometry(dsp(), window->get_icon(), &root, &x, &y, &w, &h, &border,
                 &depth);
    EXPECT_EQ(w, tiny::theme.wm_icon);
    EXPECT_EQ(h, tiny::theme.wm_icon);
}

TEST_F(WmWindowTest, wm_hints_without_icon_leave_no_icon)
{
    XWMHints hints = {};
    hints.flags = InputHint;
    hints.input = True;
    XSetWMHints(client.display, client.window, &hints);
    client.sync();

    EXPECT_EQ(manage()->get_icon(), 0u);
}

TEST_F(WmWindowTest, wm_hints_with_a_broken_icon_pixmap_leave_no_icon)
{
    XWMHints hints = {};
    hints.flags = IconPixmapHint;
    hints.icon_pixmap = 0x7fffff; // no such pixmap
    {
        XErrors errors;
        XSetWMHints(client.display, client.window, &hints);
        client.sync();
        EXPECT_EQ(manage()->get_icon(), 0u);
    }
}

TEST_F(WmWindowTest, net_wm_icon_is_converted_to_a_pixmap)
{
    client.set_net_icon(tiny::theme.wm_icon, tiny::theme.wm_icon);

    auto window = manage();

    EXPECT_NE(window->get_icon(), 0u);
    EXPECT_NE(window->get_icon_mask(), 0u);
}

TEST_F(WmWindowTest, small_net_wm_icon_is_scaled)
{
    client.set_net_icon(16, 16);

    auto window = manage();

    ASSERT_NE(window->get_icon(), 0u);
    ::Window root;
    int x, y;
    unsigned int w, h, border, depth;
    XGetGeometry(dsp(), window->get_icon(), &root, &x, &y, &w, &h, &border,
                 &depth);
    EXPECT_EQ(w, tiny::theme.wm_icon);
    EXPECT_EQ(h, tiny::theme.wm_icon);
}

TEST_F(WmWindowTest, the_best_net_wm_icon_is_chosen_from_several)
{
    std::vector<long> data = {16, 16};
    data.insert(data.end(), 16 * 16, 0xFF00FF00L);
    data.push_back(64);
    data.push_back(64);
    data.insert(data.end(), 64 * 64, 0xFF0000FFL);
    client.set_cardinals(dsp()._NET_WM_ICON, data);

    auto window = manage();

    EXPECT_NE(window->get_icon(), 0u);
}

TEST_F(WmWindowTest, icon_update_replaces_the_icon)
{
    client.set_net_icon(tiny::theme.wm_icon, tiny::theme.wm_icon);
    auto window = manage();
    window->set_events();
    ASSERT_NE(window->get_icon(), 0u);
    Pixmap first = window->get_icon();

    client.set_net_icon(32, 32);
    send(client.window, property_event(client.window, dsp()._NET_WM_ICON));

    EXPECT_NE(window->get_icon(), 0u);
    EXPECT_NE(window->get_icon(), first);
}

TEST_F(WmWindowTest, icon_name_is_read)
{
    XSetIconName(client.display, client.window, "short");
    client.sync();
    auto window = manage();
    window->set_events();

    send(client.window, property_event(client.window, dsp().WM_ICON_NAME));

    EXPECT_EQ(window->get_wm_icon_name(), "short");
}

TEST_F(WmWindowTest, net_wm_icon_name_is_preferred)
{
    client.set_utf8(dsp()._NET_WM_ICON_NAME, "dlouhé");
    auto window = manage();
    window->set_events();
    window->update_properties();

    send(client.window, property_event(client.window, dsp()._NET_WM_ICON_NAME));

    EXPECT_EQ(window->get_wm_icon_name(), "dlouhé");
}

// ---- create ----

TEST_F(WmWindowTest, create_maps_and_prepares_the_client)
{
    XWindowAttributes attrs = client.attrs();

    std::unique_ptr<WmWindow> window(
            WmWindow::create(root_window(), client.window, attrs, 0xFFFF));

    attrs = client.attrs();
    EXPECT_EQ(attrs.map_state, IsViewable);
    EXPECT_EQ(attrs.border_width, 1);
    long mask = attrs.your_event_mask; // the client's own selection
    (void)mask;
    EXPECT_TRUE(tiny::handlers.count({FocusIn, client.window}));
    EXPECT_TRUE(tiny::handlers.count({PropertyNotify, client.window}));
}

TEST_F(WmWindowTest,
       set_events_connects_all_handlers_and_destructor_removes_them)
{
    {
        auto window = manage();
        window->set_events();
        for (int type :
             {FocusIn, FocusOut, ClientMessage, PropertyNotify, KeyRelease,
              ButtonPress, ButtonRelease, MotionNotify}) {
            EXPECT_TRUE(tiny::handlers.count({type, client.window})) << type;
        }
    }

    for (int type : {FocusIn, FocusOut, ClientMessage, PropertyNotify,
                     KeyRelease, ButtonPress, ButtonRelease, MotionNotify}) {
        EXPECT_FALSE(tiny::handlers.count({type, client.window})) << type;
    }
}

TEST_F(WmWindowTest, set_events_grabs_alt_f4)
{
    auto window = manage();
    window->set_events();
    XErrors errors;

    // another client can't grab the same key: BadAccess
    XGrabKey(client.display, XKeysymToKeycode(client.display, XK_F4), Mod1Mask,
             client.window, False, GrabModeAsync, GrabModeAsync);
    client.sync();

    EXPECT_EQ(errors.errors(), 1);
}

// ---- states ----

TEST_F(WmWindowTest, set_minimized_updates_state_and_property)
{
    auto window = manage();

    window->set_minimized(true);
    EXPECT_TRUE(window->get_minimized());
    EXPECT_TRUE(client.state_has(dsp()._NET_WM_STATE_HIDDEN));

    window->set_minimized(false);
    EXPECT_FALSE(window->get_minimized());
    EXPECT_FALSE(client.state_has(dsp()._NET_WM_STATE_HIDDEN));
}

TEST_F(WmWindowTest, set_maximized_updates_state_and_property)
{
    auto window = manage();

    window->set_maximized(true);
    EXPECT_TRUE(window->get_maximized());
    EXPECT_TRUE(client.state_has(dsp()._NET_WM_STATE_MAXIMIZED_VERT));
    EXPECT_TRUE(client.state_has(dsp()._NET_WM_STATE_MAXIMIZED_HORZ));

    window->set_maximized(false);
    EXPECT_FALSE(window->get_maximized());
    EXPECT_FALSE(client.state_has(dsp()._NET_WM_STATE_MAXIMIZED_VERT));
}

TEST_F(WmWindowTest, maximize_fills_the_screen_and_restore_returns_the_size)
{
    client.map();
    auto window = manage();

    window->maximize();

    XWindowAttributes max = client.attrs();
    EXPECT_EQ(max.x, 0);
    EXPECT_EQ(max.y, 0);
    EXPECT_EQ(max.width, screen().width);
    EXPECT_EQ(max.height, screen().height);
    EXPECT_TRUE(window->get_maximized());

    window->restore();

    XWindowAttributes restored = client.attrs();
    EXPECT_EQ(restored.x, 10);
    EXPECT_EQ(restored.y, 20);
    EXPECT_EQ(restored.width, 200);
    EXPECT_EQ(restored.height, 100);
    EXPECT_FALSE(window->get_maximized());
}

TEST_F(WmWindowTest, maximize_twice_keeps_the_original_size)
{
    client.map();
    auto window = manage();

    window->maximize();
    window->maximize();
    window->restore();

    EXPECT_EQ(client.attrs().width, 200);
}

TEST_F(WmWindowTest, restore_of_maximized_window_centers_it_on_the_pointer)
{
    client.map();
    auto window = manage();
    window->maximize();

    window->restore(500, 400);

    XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(attrs.x, 500 - 200 / 2);
    EXPECT_EQ(attrs.y, 400 - 100 / 2);
}

TEST_F(WmWindowTest, minimize_hides_the_window_and_restore_shows_it)
{
    client.map();
    auto window = manage();

    window->minimize();
    EXPECT_TRUE(window->get_minimized());
    EXPECT_TRUE(client.state_has(dsp()._NET_WM_STATE_HIDDEN));

    window->minimize(); // already minimized, nothing changes
    EXPECT_TRUE(window->get_minimized());

    window->restore();
    EXPECT_FALSE(window->get_minimized());
    EXPECT_EQ(client.attrs().map_state, IsViewable);
}

// This is how a window that was maximized before the manager started comes
// back from the maximized state: its restored size must not be 1x1.
TEST_F(WmWindowTest, window_maximized_at_start_gets_a_usable_restore_size)
{
    client.map();
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});
    auto window = manage();
    ASSERT_TRUE(window->get_maximized());

    window->restore();

    EXPECT_GT(client.attrs().width, 100);
    EXPECT_GT(client.attrs().height, 100);
}

// ---- focus ----

TEST_F(WmWindowTest, set_focus_focuses_the_client_and_marks_it)
{
    client.map();
    auto window = manage();

    window->set_focus();

    ::Window focus;
    int revert;
    XGetInputFocus(dsp(), &focus, &revert);
    EXPECT_EQ(focus, client.window);
    EXPECT_TRUE(window->is_focused());
    EXPECT_TRUE(client.state_has(dsp()._NET_WM_STATE_FOCUSED));
}

TEST_F(WmWindowTest, return_focus_clears_the_focused_mark)
{
    client.map();
    auto window = manage();
    window->set_focus();

    window->return_focus();

    EXPECT_FALSE(window->is_focused());
    EXPECT_FALSE(client.state_has(dsp()._NET_WM_STATE_FOCUSED));
}

TEST_F(WmWindowTest, set_focus_restores_a_minimized_window)
{
    client.map();
    auto window = manage();
    window->minimize();

    window->set_focus();

    EXPECT_FALSE(window->get_minimized());
    EXPECT_EQ(client.attrs().map_state, IsViewable);
}

// A client that supports WM_TAKE_FOCUS must be told to take the focus.
TEST_F(WmWindowTest, set_focus_sends_wm_take_focus)
{
    client.map();
    client.set_protocols({dsp().WM_TAKE_FOCUS});
    auto window = manage();

    window->set_focus();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_TAKE_FOCUS));
}

TEST_F(WmWindowTest, set_focus_does_not_message_clients_without_take_focus)
{
    client.map();
    auto window = manage();

    window->set_focus();

    XEvent e;
    EXPECT_FALSE(client.message(e));
}

// ---- close ----

TEST_F(WmWindowTest, close_asks_a_cooperating_client_to_exit)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();

    window->close();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.message_type, dsp().WM_PROTOCOLS);
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_DELETE_WINDOW));
}

TEST_F(WmWindowTest, close_kills_a_client_without_delete_protocol)
{
    auto window = manage();

    window->close();
    XSync(dsp(), False);

    XErrors errors;
    XWindowAttributes attrs;
    XGetWindowAttributes(dsp(), client.window, &attrs);
    EXPECT_EQ(errors.errors(), 1); // BadWindow: the client is gone
    client.abandon();
}

// ---- events ----

TEST_F(WmWindowTest, focus_in_signals_on_focus)
{
    auto window = manage();
    window->set_events();
    Recorder recorder;
    window->on_focus.connect(&recorder, recorder.slot());

    send(client.window, make_event(FocusIn));

    EXPECT_EQ(recorder.calls, 1);
    EXPECT_EQ(recorder.last_owner, window.get());
}

TEST_F(WmWindowTest, focus_out_is_ignored)
{
    auto window = manage();
    window->set_events();

    send(client.window, make_event(FocusOut));

    EXPECT_FALSE(window->is_focused());
}

TEST_F(WmWindowTest, client_message_maximizes_and_restores)
{
    client.map();
    auto window = manage();
    window->set_events();

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(dsp()._NET_WM_STATE_ADD),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_VERT),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_HORZ)));
    EXPECT_TRUE(window->get_maximized());

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(dsp()._NET_WM_STATE_REMOVE),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_VERT),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_HORZ)));
    EXPECT_FALSE(window->get_maximized());
}

TEST_F(WmWindowTest, client_message_with_other_state_changes_is_ignored)
{
    client.map();
    auto window = manage();
    window->set_events();

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(dsp()._NET_WM_STATE_ADD),
                        static_cast<long>(dsp()._NET_WM_STATE_STICKY)));
    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(dsp()._NET_WM_STATE_REMOVE),
                        static_cast<long>(dsp()._NET_WM_STATE_STICKY)));
    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(dsp()._NET_WM_STATE_TOGGLE),
                        static_cast<long>(dsp()._NET_WM_STATE_STICKY)));

    EXPECT_FALSE(window->get_maximized());
}

TEST_F(WmWindowTest, client_message_can_iconify_the_window)
{
    client.map();
    auto window = manage();
    window->set_events();

    send(client.window,
         client_message(client.window, dsp().WM_CHANGE_STATE, IconicState));

    EXPECT_TRUE(window->get_minimized());
    EXPECT_EQ(client.attrs().map_state, IsUnmapped);
}

TEST_F(WmWindowTest, unknown_client_message_is_ignored)
{
    auto window = manage();
    window->set_events();

    send(client.window, client_message(client.window, dsp().WM_NAME));
    send(client.window,
         client_message(client.window, dsp().WM_CHANGE_STATE, NormalState));

    EXPECT_FALSE(window->get_minimized());
}

TEST_F(WmWindowTest, property_notify_updates_the_cached_values)
{
    auto window = manage();
    window->set_events();

    client.set_title("changed");
    send(client.window, property_event(client.window, dsp().WM_NAME));
    EXPECT_EQ(window->get_wm_name(), "changed");

    client.set_state({dsp()._NET_WM_STATE_SKIP_TASKBAR});
    send(client.window, property_event(client.window, dsp()._NET_WM_STATE));
    EXPECT_TRUE(window->skip_taskbar());

    client.set_size_hints(PPosition, 0, 0, 0, 0);
    send(client.window, property_event(client.window, dsp().WM_NORMAL_HINTS));
}

TEST_F(WmWindowTest, property_notify_updates_net_wm_name)
{
    client.set_utf8(dsp()._NET_WM_NAME, "first");
    auto window = manage();
    window->set_events();

    client.set_utf8(dsp()._NET_WM_NAME, "second");
    send(client.window, property_event(client.window, dsp()._NET_WM_NAME));

    EXPECT_EQ(window->get_wm_name(), "second");
}

// Properties that appear after the window was managed must be noticed too.
TEST_F(WmWindowTest, property_notify_reads_net_wm_name_set_later)
{
    client.set_title("plain");
    auto window = manage();
    window->set_events();

    client.set_utf8(dsp()._NET_WM_NAME, "later");
    send(client.window, property_event(client.window, dsp()._NET_WM_NAME));

    EXPECT_EQ(window->get_wm_name(), "later");
}

TEST_F(WmWindowTest, property_notify_reads_net_wm_icon_set_later)
{
    auto window = manage();
    window->set_events();

    client.set_net_icon(24, 24);
    send(client.window, property_event(client.window, dsp()._NET_WM_ICON));

    EXPECT_NE(window->get_icon(), 0u);
}

TEST_F(WmWindowTest, property_notify_updates_protocols)
{
    auto window = manage();
    window->set_events();
    client.set_protocols({dsp().WM_DELETE_WINDOW});

    send(client.window, property_event(client.window, dsp().WM_PROTOCOLS));
    window->close();

    XEvent e;
    EXPECT_TRUE(client.message(e));
}

TEST_F(WmWindowTest, property_notify_updates_the_icon)
{
    auto window = manage();
    window->set_events();
    client.set_net_icon(24, 24);
    window->update_properties();

    send(client.window, property_event(client.window, dsp()._NET_WM_ICON));

    EXPECT_NE(window->get_icon(), 0u);
}

TEST_F(WmWindowTest, property_notify_of_unknown_property_is_ignored)
{
    auto window = manage();
    window->set_events();

    send(client.window, property_event(client.window, dsp().UTF8_STRING));
}

TEST_F(WmWindowTest, alt_f4_closes_the_window)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();
    window->set_events();
    XEvent e = make_event(KeyRelease);
    e.xkey.state = Mod1Mask;
    e.xkey.keycode = XKeysymToKeycode(dsp(), XK_F4);

    send(client.window, e);

    XEvent message;
    EXPECT_TRUE(client.message(message));
}

TEST_F(WmWindowTest, other_keys_do_not_close_the_window)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();
    window->set_events();
    XEvent e = make_event(KeyRelease);
    e.xkey.state = 0;
    e.xkey.keycode = XKeysymToKeycode(dsp(), XK_F4);

    send(client.window, e);
    e.xkey.state = Mod1Mask;
    e.xkey.keycode = XKeysymToKeycode(dsp(), XK_a);
    send(client.window, e);

    XEvent message;
    EXPECT_FALSE(client.message(message));
}

TEST_F(WmWindowTest, plain_click_focuses_the_window_without_dragging)
{
    client.map();
    auto window = manage();
    window->set_events();
    Recorder dragged;
    window->on_drag_begin.connect(&dragged, dragged.slot());
    XEvent e = make_event(ButtonPress);
    e.xbutton.time = CurrentTime;

    send(client.window, e);

    EXPECT_TRUE(window->is_focused());
    EXPECT_EQ(dragged.calls, 0);
}

TEST_F(WmWindowTest, alt_click_starts_dragging_and_motion_moves_the_window)
{
    client.map();
    auto window = manage();
    window->set_events();
    XEvent press = make_event(ButtonPress);
    press.xbutton.state = Mod1Mask;
    press.xbutton.x_root = 100;
    press.xbutton.y_root = 100;
    press.xbutton.time = CurrentTime;

    send(client.window, press);

    XEvent motion = make_event(MotionNotify);
    motion.xbutton.x_root = 130;
    motion.xbutton.y_root = 150;
    send(client.window, motion);
    XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(attrs.x, 10 + 30);
    EXPECT_EQ(attrs.y, 20 + 50);

    send(client.window, make_event(ButtonRelease)); // ungrabs the pointer
}

TEST_F(WmWindowTest, dragging_a_maximized_window_restores_it_first)
{
    client.map();
    auto window = manage();
    window->maximize();
    window->set_events();
    XEvent press = make_event(ButtonPress);
    press.xbutton.state = Mod1Mask;
    press.xbutton.x = 300;
    press.xbutton.y = 200;
    press.xbutton.time = CurrentTime;

    send(client.window, press);

    EXPECT_FALSE(window->get_maximized());
    XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(attrs.width, 200);
    EXPECT_EQ(attrs.x, 300 - 200 / 2);
    send(client.window, make_event(ButtonRelease));
}

TEST_F(WmWindowTest, icon_update_without_mask_causes_no_x_error)
{
    client.set_icon_pixmap(tiny::theme.wm_icon, true);
    XErrors errors;
    {
        auto window = manage();
        client.set_icon_pixmap(tiny::theme.wm_icon, false);
        window->update_wm_hints();
    }
    EXPECT_EQ(errors.errors(), 0);
}
