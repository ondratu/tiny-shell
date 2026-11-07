#include "fake_client.h"
#include "wm_helpers.h"
#include "wm/shell_theme.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <memory>
#include <string>

#include "wm/process.h"
#include "wm/window.h"

using tiny::wm::MotifWMHints;
using WmWindow = tiny::wm::Window;

namespace {

class WindowTest : public ::testing::Test {
  protected:
    FakeClient client;

    // Screen size, used by maximize.
    XWindowAttributes screen() const { return attributes_of(root_window()); }

    std::unique_ptr<WmWindow> manage(unsigned long functions = 0xFFFF)
    {
        return std::make_unique<WmWindow>(client.window, root_window(),
                                          functions, screen_area());
    }
};

// The time of an optional that is there; the beginning of time when it is not
WmWindow::Clock::time_point ask_time(const WmWindow& window)
{
    return window.get_ask_time().value_or(WmWindow::Clock::time_point{});
}

WmWindow::Clock::time_point ping_wakeup(const WmWindow& window)
{
    return window.get_ping_wakeup().value_or(WmWindow::Clock::time_point{});
}

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

TEST_F(WindowTest, two_clicks_close_in_time_are_a_double_click)
{
    EXPECT_TRUE(tiny::wm::is_double_click(true, 1000, 1300));
    EXPECT_FALSE(tiny::wm::is_double_click(true, 1000, 1500));
    EXPECT_FALSE(tiny::wm::is_double_click(false, 0, 10));
}

TEST_F(WindowTest, the_time_of_a_double_click_wraps_around)
{
    EXPECT_TRUE(tiny::wm::is_double_click(true, 0xFFFFFF00UL, 0x20UL));
}

TEST_F(WindowTest, an_old_client_without_user_time_takes_the_focus)
{
    EXPECT_TRUE(tiny::wm::takes_focus(false, 0, true, 1000));
}

TEST_F(WindowTest, user_time_zero_never_takes_the_focus)
{
    EXPECT_FALSE(tiny::wm::takes_focus(true, 0, false, 0));
    EXPECT_FALSE(tiny::wm::takes_focus(true, 0, true, 1000));
}

TEST_F(WindowTest, a_window_older_than_the_last_action_does_not_take_the_focus)
{
    EXPECT_FALSE(tiny::wm::takes_focus(true, 999, true, 1000));
    EXPECT_TRUE(tiny::wm::takes_focus(true, 1000, true, 1000));
    EXPECT_TRUE(tiny::wm::takes_focus(true, 1001, true, 1000));
}

TEST_F(WindowTest, without_a_known_action_a_window_with_a_time_takes_the_focus)
{
    EXPECT_TRUE(tiny::wm::takes_focus(true, 5, false, 0));
}

TEST_F(WindowTest, the_user_time_wraps_around_like_the_server_time)
{
    EXPECT_TRUE(tiny::wm::takes_focus(true, 10, true, 0xFFFFFFF0UL));
    EXPECT_FALSE(tiny::wm::takes_focus(true, 0xFFFFFFF0UL, true, 10));
}

TEST_F(WindowTest, motif_all_bit_turns_the_listed_bits_around)
{
    unsigned long functions = 0;
    unsigned long decorations = 0;
    const unsigned long hints[] = {
            MotifWMHints::FLAG_FUNCTIONS | MotifWMHints::FLAG_DECORATIONS,
            MotifWMHints::FUNC_ALL | MotifWMHints::FUNC_CLOSE,
            MotifWMHints::DECOR_ALL | MotifWMHints::DECOR_MAXIMIZE};

    tiny::wm::resolve_motif_hints(hints, 3, functions, decorations);

    EXPECT_FALSE(functions & MotifWMHints::FUNC_CLOSE);
    EXPECT_TRUE(functions & MotifWMHints::FUNC_MINIMIZE);
    EXPECT_FALSE(functions & MotifWMHints::FUNC_ALL);
    EXPECT_FALSE(decorations & MotifWMHints::DECOR_MAXIMIZE);
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_TITLE);
    EXPECT_FALSE(decorations & MotifWMHints::DECOR_ALL);
}

TEST_F(WindowTest, motif_bits_without_all_are_the_allowed_ones)
{
    unsigned long functions = 0;
    unsigned long decorations = 0;
    const unsigned long hints[] = {
            MotifWMHints::FLAG_FUNCTIONS | MotifWMHints::FLAG_DECORATIONS,
            MotifWMHints::FUNC_MOVE, MotifWMHints::DECOR_TITLE};

    tiny::wm::resolve_motif_hints(hints, 3, functions, decorations);

    EXPECT_EQ(functions, static_cast<unsigned long>(MotifWMHints::FUNC_MOVE));
    EXPECT_EQ(decorations,
              static_cast<unsigned long>(MotifWMHints::DECOR_TITLE));
}

TEST_F(WindowTest, a_short_motif_property_is_not_read_past_its_end)
{
    unsigned long functions = 0;
    unsigned long decorations = 0;
    // the flags say functions and decorations, the property holds the flags
    // and the functions only
    const unsigned long hints[] = {MotifWMHints::FLAG_FUNCTIONS |
                                           MotifWMHints::FLAG_DECORATIONS,
                                   MotifWMHints::FUNC_CLOSE};

    tiny::wm::resolve_motif_hints(hints, 2, functions, decorations);

    EXPECT_EQ(functions, static_cast<unsigned long>(MotifWMHints::FUNC_CLOSE));
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_TITLE); // not set: all
}

TEST_F(WindowTest, motif_hints_of_another_format_are_ignored)
{
    const unsigned char bytes[5] = {1, 0, 0, 0, 0};
    XChangeProperty(client.display, client.window, dsp()._MOTIF_WM_HINTS,
                    dsp()._MOTIF_WM_HINTS, 8, PropModeReplace, bytes, 5);
    client.sync();
    unsigned long functions = 0;
    unsigned long decorations = 0;

    EXPECT_FALSE(
            WmWindow::get_motif_hints(client.window, functions, decorations));
    EXPECT_TRUE(functions & MotifWMHints::FUNC_CLOSE);
}

TEST_F(WindowTest, no_motif_data_allows_everything)
{
    unsigned long functions = 0;
    unsigned long decorations = 0;

    tiny::wm::resolve_motif_hints(nullptr, 0, functions, decorations);

    EXPECT_TRUE(functions & MotifWMHints::FUNC_MINIMIZE);
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_MAXIMIZE);
}

TEST_F(WindowTest, motif_hints_default_to_everything_allowed)
{
    unsigned long functions = 0;
    unsigned long decorations = 0;

    EXPECT_FALSE(
            WmWindow::get_motif_hints(client.window, functions, decorations));

    EXPECT_TRUE(functions & MotifWMHints::FUNC_RESIZE);
    EXPECT_TRUE(functions & MotifWMHints::FUNC_CLOSE);
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_TITLE);
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_BORDER);
}

TEST_F(WindowTest, motif_hints_without_decorations)
{
    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0, 0);
    unsigned long functions = 0;
    unsigned long decorations = 1;

    EXPECT_TRUE(
            WmWindow::get_motif_hints(client.window, functions, decorations));

    EXPECT_EQ(decorations, 0u);
    EXPECT_TRUE(functions & MotifWMHints::FUNC_MOVE); // not overridden
}

TEST_F(WindowTest, motif_hints_override_functions)
{
    client.set_motif(MotifWMHints::FLAG_FUNCTIONS, MotifWMHints::FUNC_CLOSE, 0);
    unsigned long functions = 0;
    unsigned long decorations = 0;

    EXPECT_TRUE(
            WmWindow::get_motif_hints(client.window, functions, decorations));

    EXPECT_EQ(functions, static_cast<unsigned long>(MotifWMHints::FUNC_CLOSE));
    EXPECT_TRUE(decorations & MotifWMHints::DECOR_TITLE); // not overridden
}

TEST_F(WindowTest, net_wm_type_defaults_to_normal)
{
    EXPECT_EQ(WmWindow::get_net_wm_type(client.window),
              WmWindow::WMType::NORMAL);
}

TEST_F(WindowTest, net_wm_type_is_read_from_the_property)
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
            {dsp()._NET_WM_WINDOW_TYPE_NOTIFICATION, WmWindow::WMType::POPUP},
            {dsp()._NET_WM_WINDOW_TYPE_POPUP_MENU, WmWindow::WMType::POPUP},
            {dsp()._NET_WM_WINDOW_TYPE_DROPDOWN_MENU, WmWindow::WMType::POPUP},
            {dsp()._NET_WM_WINDOW_TYPE_TOOLTIP, WmWindow::WMType::POPUP},
            {dsp()._NET_WM_WINDOW_TYPE_COMBO, WmWindow::WMType::POPUP},
            {dsp()._NET_WM_WINDOW_TYPE_DND, WmWindow::WMType::POPUP},
            {dsp()._NET_WM_WINDOW_TYPE_NORMAL, WmWindow::WMType::NORMAL},
    };

    for (auto& c : cases) {
        client.set_atoms(dsp()._NET_WM_WINDOW_TYPE, {c.atom});
        EXPECT_EQ(WmWindow::get_net_wm_type(client.window), c.type);
    }
}

TEST_F(WindowTest, net_wm_type_uses_the_first_known_type)
{
    client.set_atoms(dsp()._NET_WM_WINDOW_TYPE,
                     {dsp().WM_NAME, dsp()._NET_WM_WINDOW_TYPE_DIALOG,
                      dsp()._NET_WM_WINDOW_TYPE_DOCK});

    EXPECT_EQ(WmWindow::get_net_wm_type(client.window),
              WmWindow::WMType::DIALOG);
}

TEST_F(WindowTest, get_properties_lists_window_properties)
{
    client.set_title("title");
    client.set_state({});
    std::set<Atom> properties;

    const int count = WmWindow::get_properties(client.window, properties);

    EXPECT_EQ(count, static_cast<int>(properties.size()));
    EXPECT_TRUE(properties.count(dsp().WM_NAME));
    EXPECT_TRUE(properties.count(dsp()._NET_WM_STATE));
}

TEST_F(WindowTest, get_properties_of_window_without_properties)
{
    std::set<Atom> properties = {dsp().WM_NAME};

    EXPECT_EQ(WmWindow::get_properties(client.window, properties), 0);

    EXPECT_TRUE(properties.empty());
}

// ---- construction ----

TEST_F(WindowTest, plain_window_has_no_special_state)
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

TEST_F(WindowTest, wm_class_is_read)
{
    client.set_class("reto", "Reto");

    auto window = manage();

    EXPECT_EQ(window->get_wm_class_instance(), "reto");
    EXPECT_EQ(window->get_wm_class(), "Reto");
}

TEST_F(WindowTest, the_application_id_is_set_by_the_manager)
{
    auto window = manage();
    EXPECT_EQ(window->get_app_id(), "");

    window->set_app_id("reto.desktop");

    EXPECT_EQ(window->get_app_id(), "reto.desktop");
}

TEST_F(WindowTest, window_without_wm_class_has_empty_names)
{
    auto window = manage();

    EXPECT_EQ(window->get_wm_class_instance(), "");
    EXPECT_EQ(window->get_wm_class(), "");
    EXPECT_EQ(window->get_app_icon().picture, None);
}

TEST_F(WindowTest, resize_and_maximize_depend_on_functions)
{
    auto window = manage(MotifWMHints::FUNC_RESIZE);
    EXPECT_TRUE(window->is_resizable());
    EXPECT_FALSE(window->is_maximizable());

    window = manage(MotifWMHints::FUNC_MAXIMIZE);
    EXPECT_FALSE(window->is_resizable());
    EXPECT_TRUE(window->is_maximizable());
}

TEST_F(WindowTest, initial_wm_states_are_read)
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

TEST_F(WindowTest, all_wm_states_round_trip_through_the_property)
{
    const std::vector<Atom> all = {dsp()._NET_WM_STATE_MODAL,
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

TEST_F(WindowTest, unknown_wm_state_is_ignored)
{
    client.set_state({dsp().WM_NAME, dsp()._NET_WM_STATE_STICKY});

    auto window = manage();
    std::vector<Atom> atoms;
    window->get_wm_states(atoms);

    EXPECT_EQ(atoms, std::vector<Atom>{dsp()._NET_WM_STATE_STICKY});
}

TEST_F(WindowTest, empty_wm_state_property_clears_the_state)
{
    client.set_state({dsp()._NET_WM_STATE_HIDDEN});
    auto window = manage();
    ASSERT_TRUE(window->get_minimized());
    client.set_state({});

    window->update_wm_states();

    EXPECT_FALSE(window->get_minimized());
}

TEST_F(WindowTest, title_is_read_from_wm_name)
{
    client.set_title("old style");

    EXPECT_EQ(manage()->get_wm_name(), "old style");
}

TEST_F(WindowTest, net_wm_name_wins_over_wm_name)
{
    client.set_title("old style");
    client.set_utf8(dsp()._NET_WM_NAME, "ěšč");

    EXPECT_EQ(manage()->get_wm_name(), "ěšč");
}

TEST_F(WindowTest, normal_hints_without_limits_keep_resizing)
{
    // like kitty: only the gravity is set
    client.set_size_hints(PWinGravity, 0, 0, 0, 0);

    auto window =
            manage(MotifWMHints::FUNC_RESIZE | MotifWMHints::FUNC_MAXIMIZE);

    EXPECT_TRUE(window->is_resizable());
    EXPECT_TRUE(window->is_maximizable());
}

TEST_F(WindowTest, normal_hints_with_equal_limits_disable_resizing)
{
    client.set_size_hints(PMinSize | PMaxSize, 300, 200, 300, 200);

    auto window =
            manage(MotifWMHints::FUNC_RESIZE | MotifWMHints::FUNC_MAXIMIZE);

    EXPECT_FALSE(window->is_resizable());
    EXPECT_FALSE(window->is_maximizable());
}

TEST_F(WindowTest, normal_hints_with_range_keep_resizing)
{
    client.set_size_hints(PMinSize | PMaxSize, 100, 100, 800, 600);

    auto window =
            manage(MotifWMHints::FUNC_RESIZE | MotifWMHints::FUNC_MAXIMIZE);

    EXPECT_TRUE(window->is_resizable());
    EXPECT_TRUE(window->is_maximizable());
}

TEST_F(WindowTest, user_time_changes_are_not_reported)
{
    auto window = manage();
    window->set_events();

    ::testing::internal::CaptureStderr();
    send(client.window, property_event(client.window, dsp()._NET_WM_USER_TIME));
    const std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_EQ(out.find("Unhandled"), std::string::npos);
}

TEST_F(WindowTest, wm_state_changes_are_not_reported)
{
    auto window = manage();
    window->set_events();

    ::testing::internal::CaptureStderr();
    send(client.window, property_event(client.window, dsp().WM_STATE));
    const std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_EQ(out.find("Unhandled"), std::string::npos);
}

// ---- size hints ----

namespace {

// A terminal: cells of 10x20 pixels, 4 and 6 pixels around, at least 2x2 cells
XSizeHints terminal_hints()
{
    XSizeHints hints = {};
    hints.flags = PMinSize | PBaseSize | PResizeInc;
    hints.base_width = 4;
    hints.base_height = 6;
    hints.min_width = 4 + 2 * 10;
    hints.min_height = 6 + 2 * 20;
    hints.width_inc = 10;
    hints.height_inc = 20;
    return hints;
}

} // namespace

// ---- aspect ratio ----

namespace {

// width / height between min_x:min_y and max_x:max_y
XSizeHints aspect_hints(int min_x, int min_y, int max_x, int max_y)
{
    XSizeHints hints = {};
    hints.flags = PAspect;
    hints.min_aspect.x = min_x;
    hints.min_aspect.y = min_y;
    hints.max_aspect.x = max_x;
    hints.max_aspect.y = max_y;
    return hints;
}

} // namespace

TEST(AspectTest, hints_without_a_ratio_leave_the_size_alone)
{
    const XSizeHints hints = {};
    int width = 400;
    int height = 100;

    tiny::wm::fit_aspect(hints, width, height, true);

    EXPECT_EQ(width, 400);
    EXPECT_EQ(height, 100);
}

TEST(AspectTest, a_size_inside_the_ratio_is_left_alone)
{
    const XSizeHints hints = aspect_hints(1, 2, 2, 1);
    int width = 300;
    int height = 200;

    tiny::wm::fit_aspect(hints, width, height, true);

    EXPECT_EQ(width, 300);
    EXPECT_EQ(height, 200);
}

TEST(AspectTest, a_window_that_is_too_wide_gets_taller_or_narrower)
{
    const XSizeHints hints = aspect_hints(1, 1, 2, 1);
    int width = 400;
    int height = 100;
    tiny::wm::fit_aspect(hints, width, height, true);
    EXPECT_EQ(width, 400);
    EXPECT_EQ(height, 200);

    width = 400;
    height = 100;
    tiny::wm::fit_aspect(hints, width, height, false);
    EXPECT_EQ(width, 200);
    EXPECT_EQ(height, 100);
}

TEST(AspectTest, a_window_that_is_too_tall_gets_shorter_or_wider)
{
    const XSizeHints hints = aspect_hints(1, 1, 2, 1);
    int width = 100;
    int height = 400;
    tiny::wm::fit_aspect(hints, width, height, true);
    EXPECT_EQ(width, 100);
    EXPECT_EQ(height, 100);

    width = 100;
    height = 400;
    tiny::wm::fit_aspect(hints, width, height, false);
    EXPECT_EQ(width, 400);
    EXPECT_EQ(height, 400);
}

TEST(AspectTest, the_ratio_counts_from_the_base_size)
{
    XSizeHints hints = aspect_hints(1, 1, 1, 1);
    hints.flags |= PBaseSize;
    hints.base_width = 20;
    hints.base_height = 10;
    int width = 120; // 100 over the base
    int height = 60; // 50 over the base

    tiny::wm::fit_aspect(hints, width, height, true);

    EXPECT_EQ(width, 120);
    EXPECT_EQ(height, 110);
}

TEST(AspectTest, the_limits_of_the_client_win_over_the_ratio)
{
    XSizeHints hints = aspect_hints(1, 1, 1, 1);
    hints.flags |= PMaxSize;
    hints.max_width = 1000;
    hints.max_height = 150;
    int width = 400;
    int height = 100;

    tiny::wm::fit_aspect(hints, width, height, false);

    EXPECT_EQ(width, 100);
    hints.flags = PAspect | PMinSize | PBaseSize;
    hints.min_width = 250;
    hints.min_height = 0;
    hints.base_width = 0;
    hints.base_height = 0;
    width = 400;
    height = 100;
    tiny::wm::fit_aspect(hints, width, height, false);
    EXPECT_EQ(width, 250);
}

TEST(AspectTest, an_unset_side_of_the_ratio_is_not_a_limit)
{
    const XSizeHints hints = aspect_hints(0, 0, 0, 0);
    int width = 400;
    int height = 100;

    tiny::wm::fit_aspect(hints, width, height, true);

    EXPECT_EQ(width, 400);
    EXPECT_EQ(height, 100);
}

TEST(ResizeByEdgesTest, a_ratio_follows_the_dragged_side)
{
    const XSizeHints hints = aspect_hints(16, 9, 16, 9);

    const tiny::Rect right = tiny::wm::resize_by_edges(
            hints, {0, 0, 320, 180}, tiny::Position::Right, 160, 37, 0, 10, 10);
    EXPECT_EQ(right.width, 480);
    EXPECT_EQ(right.height, 270);

    const tiny::Rect bottom = tiny::wm::resize_by_edges(
            hints, {0, 0, 320, 180}, tiny::Position::Bottom, 37, 90, 0, 10, 10);
    EXPECT_EQ(bottom.width, 480);
    EXPECT_EQ(bottom.height, 270);
}

TEST(ResizeByEdgesTest, a_ratio_keeps_the_opposite_corner_in_place)
{
    const XSizeHints hints = aspect_hints(16, 9, 16, 9);

    const tiny::Rect r = tiny::wm::resize_by_edges(
            hints, {100, 100, 320, 180},
            tiny::Position::Left | tiny::Position::Top, -160, -20, 0, 10, 10);

    EXPECT_EQ(r.width, 480); // the horizontal move is the larger one
    EXPECT_EQ(r.height, 270);
    EXPECT_EQ(r.x + r.width, 420);
    EXPECT_EQ(r.y + r.height, 280);
}

TEST(ResizeByEdgesTest, the_grid_and_the_minimum_win_over_the_ratio)
{
    XSizeHints hints = aspect_hints(1, 1, 1, 1);
    hints.flags |= PResizeInc | PBaseSize;
    hints.width_inc = 10;
    hints.height_inc = 10;
    hints.base_width = 0;
    hints.base_height = 0;

    const tiny::Rect r = tiny::wm::resize_by_edges(hints, {0, 0, 200, 200},
                                                   tiny::Position::Bottom, 0,
                                                   -195, 0, 80, 80);

    EXPECT_GE(r.width, 80);
    EXPECT_GE(r.height, 80);
    EXPECT_EQ(r.width % 10, 0);
    EXPECT_EQ(r.height % 10, 0);
}

TEST(ResizeByEdgesTest, a_ratio_does_not_count_the_title_bar)
{
    const XSizeHints hints = aspect_hints(2, 1, 2, 1);

    const tiny::Rect r = tiny::wm::resize_by_edges(
            hints, {0, 0, 200, 124}, tiny::Position::Right, 100, 0, 24, 10, 10);

    EXPECT_EQ(r.width, 300);
    EXPECT_EQ(r.height, 150 + 24);
}

// ---- gravity ----

namespace {

// 2 pixels of border on each side and 26 of title bar
tiny::wm::FrameExtents titled_extents()
{
    tiny::wm::FrameExtents e;
    e.left = 2;
    e.right = 2;
    e.top = 26;
    e.bottom = 2;
    return e;
}

} // namespace

TEST(GravityTest, the_reference_point_of_the_frame_is_placed_at_the_position)
{
    const tiny::wm::FrameExtents e = titled_extents();
    using P = std::pair<int, int>;

    EXPECT_EQ(tiny::wm::frame_position(NorthWestGravity, 100, 100, e),
              P(100, 100));
    EXPECT_EQ(tiny::wm::frame_position(NorthGravity, 100, 100, e), P(98, 100));
    EXPECT_EQ(tiny::wm::frame_position(NorthEastGravity, 100, 100, e),
              P(96, 100));
    EXPECT_EQ(tiny::wm::frame_position(WestGravity, 100, 100, e), P(100, 86));
    EXPECT_EQ(tiny::wm::frame_position(CenterGravity, 100, 100, e), P(98, 86));
    EXPECT_EQ(tiny::wm::frame_position(EastGravity, 100, 100, e), P(96, 86));
    EXPECT_EQ(tiny::wm::frame_position(SouthWestGravity, 100, 100, e),
              P(100, 72));
    EXPECT_EQ(tiny::wm::frame_position(SouthGravity, 100, 100, e), P(98, 72));
    EXPECT_EQ(tiny::wm::frame_position(SouthEastGravity, 100, 100, e),
              P(96, 72));
}

TEST(GravityTest, static_gravity_places_the_client_itself)
{
    EXPECT_EQ(
            tiny::wm::frame_position(StaticGravity, 100, 100, titled_extents()),
            std::make_pair(98, 74));
}

TEST(GravityTest, forget_gravity_is_north_west)
{
    EXPECT_EQ(
            tiny::wm::frame_position(ForgetGravity, 100, 100, titled_extents()),
            std::make_pair(100, 100));
}

TEST(GravityTest, a_frame_without_extents_is_not_moved)
{
    for (int gravity = ForgetGravity; gravity <= StaticGravity; ++gravity) {
        EXPECT_EQ(tiny::wm::frame_position(gravity, 7, 9, {}),
                  std::make_pair(7, 9));
    }
}

TEST(GravityTest, a_growing_window_keeps_its_reference_point)
{
    using P = std::pair<int, int>;

    EXPECT_EQ(tiny::wm::gravity_shift(NorthWestGravity, 10, 20), P(0, 0));
    EXPECT_EQ(tiny::wm::gravity_shift(NorthGravity, 10, 20), P(-5, 0));
    EXPECT_EQ(tiny::wm::gravity_shift(NorthEastGravity, 10, 20), P(-10, 0));
    EXPECT_EQ(tiny::wm::gravity_shift(WestGravity, 10, 20), P(0, -10));
    EXPECT_EQ(tiny::wm::gravity_shift(CenterGravity, 10, 20), P(-5, -10));
    EXPECT_EQ(tiny::wm::gravity_shift(EastGravity, 10, 20), P(-10, -10));
    EXPECT_EQ(tiny::wm::gravity_shift(SouthWestGravity, 10, 20), P(0, -20));
    EXPECT_EQ(tiny::wm::gravity_shift(SouthGravity, 10, 20), P(-5, -20));
    EXPECT_EQ(tiny::wm::gravity_shift(SouthEastGravity, 10, 20), P(-10, -20));
    EXPECT_EQ(tiny::wm::gravity_shift(StaticGravity, 10, 20), P(0, 0));
}

TEST(GravityTest, a_shrinking_window_moves_the_other_way)
{
    EXPECT_EQ(tiny::wm::gravity_shift(SouthEastGravity, -10, -20),
              std::make_pair(10, 20));
}

TEST(GravityTest, the_gravity_of_the_hints_is_north_west_without_a_flag)
{
    XSizeHints hints = {};
    hints.win_gravity = SouthGravity;
    EXPECT_EQ(tiny::wm::window_gravity(hints), NorthWestGravity);

    hints.flags = PWinGravity;
    EXPECT_EQ(tiny::wm::window_gravity(hints), SouthGravity);
}

TEST(ResizeByEdgesTest, the_right_and_bottom_edges_grow_the_frame)
{
    const XSizeHints hints = {};

    const tiny::Rect r = tiny::wm::resize_by_edges(
            hints, {100, 50, 300, 200},
            tiny::Position::Right | tiny::Position::Bottom, 20, 30, 24, 100,
            100);

    EXPECT_EQ(r.x, 100);
    EXPECT_EQ(r.y, 50);
    EXPECT_EQ(r.width, 320);
    EXPECT_EQ(r.height, 230);
}

TEST(ResizeByEdgesTest, the_left_and_top_edges_move_the_frame)
{
    const XSizeHints hints = {};

    const tiny::Rect r = tiny::wm::resize_by_edges(
            hints, {100, 50, 300, 200},
            tiny::Position::Left | tiny::Position::Top, -20, -30, 24, 100, 100);

    EXPECT_EQ(r.x, 80);
    EXPECT_EQ(r.y, 20);
    EXPECT_EQ(r.width, 320);
    EXPECT_EQ(r.height, 230);
}

TEST(ResizeByEdgesTest, an_edge_that_is_not_dragged_does_not_move)
{
    const XSizeHints hints = {};

    const tiny::Rect r = tiny::wm::resize_by_edges(hints, {100, 50, 300, 200},
                                                   tiny::Position::Right, 20,
                                                   99, 24, 100, 100);

    EXPECT_EQ(r.y, 50);
    EXPECT_EQ(r.height, 200);
    EXPECT_EQ(r.width, 320);
}

TEST(ResizeByEdgesTest,
     the_dragged_edge_stops_at_the_minimum_and_the_other_stays)
{
    const XSizeHints hints = {};

    const tiny::Rect r = tiny::wm::resize_by_edges(
            hints, {100, 50, 300, 200},
            tiny::Position::Left | tiny::Position::Top, 500, 500, 24, 120, 90);

    EXPECT_EQ(r.width, 120);
    EXPECT_EQ(r.height, 90);
    EXPECT_EQ(r.x + r.width, 400);
    EXPECT_EQ(r.y + r.height, 250);
}

TEST(ResizeByEdgesTest, the_size_hints_are_followed)
{
    XSizeHints hints = {};
    hints.flags = PMaxSize | PResizeInc | PBaseSize;
    hints.max_width = 500;
    hints.max_height = 500;
    hints.width_inc = 10;
    hints.height_inc = 10;
    hints.base_width = 0;
    hints.base_height = 0;

    const tiny::Rect r = tiny::wm::resize_by_edges(
            hints, {0, 0, 300, 224},
            tiny::Position::Right | tiny::Position::Bottom, 1000, 37, 24, 100,
            100);

    EXPECT_EQ(r.width, 500);
    EXPECT_EQ(r.height, 24 + 230);
}

TEST(SizeHintsTest, no_hints_leave_the_size_as_it_is)
{
    const XSizeHints hints = {};

    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 333), 333);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, false, 1), 1);
    EXPECT_FALSE(tiny::wm::has_resize_increment(hints));
}

TEST(SizeHintsTest, size_snaps_down_to_base_plus_whole_increments)
{
    const XSizeHints hints = terminal_hints();

    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 4 + 79 * 10 + 9),
              4 + 79 * 10);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, false, 6 + 22 * 20 + 19),
              6 + 22 * 20);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 4 + 5 * 10), 4 + 5 * 10);
}

TEST(SizeHintsTest, size_snaps_even_when_the_start_is_not_on_the_grid)
{
    const XSizeHints hints = terminal_hints();

    // 912 pixels is not 40 + n * 38: the last row would be a gray strip
    XSizeHints tall = hints;
    tall.base_height = 40;
    tall.height_inc = 38;
    tall.min_height = 78;
    EXPECT_EQ(tiny::wm::fit_client_size(tall, false, 912), 40 + 22 * 38);
}

TEST(SizeHintsTest, floor_of_the_window_manager_is_kept_on_the_grid)
{
    XSizeHints hints = {};
    hints.flags = PBaseSize | PResizeInc;
    hints.base_width = 3;
    hints.width_inc = 7;

    // the first grid size 3 + n * 7 that is not under 10 is 10
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 4, 10), 10);
    // 11 is not on the grid: 17 is the first one not under it
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 4, 11), 17);
    // sizes over the floor are only snapped down
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 30, 10), 3 + 3 * 7);
}

TEST(SizeHintsTest, floor_without_increment_is_the_minimum)
{
    const XSizeHints hints = {};

    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 4, 10), 10);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 50, 10), 50);
}

TEST(SizeHintsTest, maximum_of_the_client_wins_over_the_floor)
{
    XSizeHints hints = {};
    hints.flags = PMaxSize;
    hints.max_width = 6;

    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 4, 10), 6);
}

TEST(SizeHintsTest, maximum_on_the_grid_wins_over_the_floor)
{
    XSizeHints hints = {};
    hints.flags = PBaseSize | PResizeInc | PMaxSize;
    hints.base_width = 3;
    hints.width_inc = 7;
    hints.max_width = 12;

    // 10 is the only grid size that fits in 12
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 4, 12), 10);
}

TEST(SizeHintsTest, size_is_limited_by_min_and_max_on_the_grid)
{
    XSizeHints hints = terminal_hints();
    hints.flags |= PMaxSize;
    hints.max_width = 4 + 100 * 10;
    hints.max_height = 6 + 40 * 20;

    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 5000), 4 + 100 * 10);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 0), 4 + 2 * 10);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, false, 5000), 6 + 40 * 20);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, false, 0), 6 + 2 * 20);
}

TEST(SizeHintsTest, min_size_without_increment_is_exact)
{
    XSizeHints hints = {};
    hints.flags = PMinSize;
    hints.min_width = 120;
    hints.min_height = 90;

    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 100), 120);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, false, 500), 500);
}

TEST(SizeHintsTest, base_size_stands_for_the_min_size_and_the_other_way)
{
    XSizeHints hints = {};
    hints.flags = PMinSize | PResizeInc; // base is the min size
    hints.min_width = 50;
    hints.min_height = 50;
    hints.width_inc = 10;
    hints.height_inc = 10;

    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 97), 90);
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 10), 50);

    hints.flags = PBaseSize | PResizeInc; // min is the base size
    hints.base_width = 3;
    hints.base_height = 3;
    EXPECT_EQ(tiny::wm::fit_client_size(hints, true, 1), 3);
}

TEST(SizeHintsTest, units_count_the_increments_over_the_base)
{
    const XSizeHints hints = terminal_hints();

    EXPECT_EQ(tiny::wm::client_size_units(hints, true, 4 + 80 * 10), 80);
    EXPECT_EQ(tiny::wm::client_size_units(hints, false, 6 + 24 * 20), 24);
    EXPECT_TRUE(tiny::wm::has_resize_increment(hints));
}

TEST(SizeHintsTest, increment_of_one_pixel_is_not_a_unit)
{
    XSizeHints hints = {};
    hints.flags = PResizeInc;
    hints.width_inc = 1;
    hints.height_inc = 1;

    EXPECT_FALSE(tiny::wm::has_resize_increment(hints));
}

TEST_F(WindowTest, window_manager_protocols_are_read)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();

    window->close();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_DELETE_WINDOW));
}

// ---- _NET_WM_SYNC_REQUEST ----

TEST_F(WindowTest, a_window_without_a_counter_is_never_waited_for)
{
    auto window = manage();

    EXPECT_EQ(window->get_sync_counter(), None);
    EXPECT_TRUE(window->sync_allows_resize());
    window->sync_request_resize(); // nothing is sent

    XEvent e;
    EXPECT_FALSE(client.message(e));
    EXPECT_TRUE(window->sync_allows_resize());
}

TEST_F(WindowTest, the_counter_is_used_only_when_the_protocol_is_there)
{
    const XSyncCounter counter = client.set_sync_counter();
    client.set_protocols({dsp().WM_DELETE_WINDOW}); // not _NET_WM_SYNC_REQUEST
    auto window = manage();

    EXPECT_EQ(window->get_sync_counter(), None);
    XSyncDestroyCounter(client.display, counter);
}

TEST_F(WindowTest, the_counter_of_the_client_is_read)
{
    const XSyncCounter counter = client.set_sync_counter();

    auto window = manage();

    EXPECT_EQ(window->get_sync_counter(), counter);
}

TEST_F(WindowTest, a_request_waits_for_the_client_to_draw)
{
    const XSyncCounter counter = client.set_sync_counter();
    auto window = manage();
    ASSERT_TRUE(window->sync_allows_resize());

    window->sync_request_resize();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.message_type, dsp().WM_PROTOCOLS);
    EXPECT_EQ(e.xclient.data.l[0],
              static_cast<long>(dsp()._NET_WM_SYNC_REQUEST));
    EXPECT_EQ(e.xclient.data.l[2], 1); // the number of the request
    EXPECT_EQ(e.xclient.data.l[3], 0);
    EXPECT_FALSE(window->sync_allows_resize());

    client.set_counter(counter, 1); // drawn

    EXPECT_TRUE(window->sync_allows_resize());
}

TEST_F(WindowTest, the_next_request_has_the_next_number)
{
    const XSyncCounter counter = client.set_sync_counter();
    auto window = manage();
    window->sync_request_resize();
    client.set_counter(counter, 1);

    window->sync_request_resize();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[2], 2);
}

TEST_F(WindowTest, a_new_resize_does_not_wait_for_the_last_one)
{
    client.set_sync_counter();
    auto window = manage();
    window->sync_request_resize();
    ASSERT_FALSE(window->sync_allows_resize());

    window->sync_reset();

    EXPECT_TRUE(window->sync_allows_resize());
}

TEST_F(WindowTest, a_counter_that_is_gone_is_not_waited_for)
{
    const XSyncCounter counter = client.set_sync_counter();
    auto window = manage();
    window->sync_request_resize();
    XSyncDestroyCounter(client.display, counter);
    client.sync();

    EXPECT_TRUE(window->sync_allows_resize());
    EXPECT_EQ(window->get_sync_counter(), None);
}

TEST_F(WindowTest, a_counter_that_comes_later_is_found)
{
    auto window = manage();
    window->set_events();
    ASSERT_EQ(window->get_sync_counter(), None);

    const XSyncCounter counter = client.set_sync_counter();
    // the two properties come in any order, the counter is found by the last
    send(client.window,
         property_event(client.window, dsp()._NET_WM_SYNC_REQUEST_COUNTER));
    send(client.window, property_event(client.window, dsp().WM_PROTOCOLS));

    EXPECT_EQ(window->get_sync_counter(), counter);
}

// ---- _NET_WM_MOVERESIZE ----

TEST(MoveResizeTest, the_directions_name_the_edges)
{
    using tiny::Position;
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_TOPLEFT),
              unsigned(Position::Top | Position::Left));
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_TOP),
              unsigned(Position::Top));
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_TOPRIGHT),
              unsigned(Position::Top | Position::Right));
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_RIGHT),
              unsigned(Position::Right));
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_BOTTOMRIGHT),
              unsigned(Position::Bottom | Position::Right));
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_BOTTOM),
              unsigned(Position::Bottom));
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_BOTTOMLEFT),
              unsigned(Position::Bottom | Position::Left));
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_LEFT),
              unsigned(Position::Left));
}

TEST(MoveResizeTest, moving_and_the_rest_drag_no_edge)
{
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::MOVE_BY_POINTER), 0u);
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::SIZE_KEYBOARD), 0u);
    EXPECT_EQ(tiny::wm::edges_of_direction(tiny::wm::MOVE_RESIZE_CANCEL), 0u);
    EXPECT_EQ(tiny::wm::edges_of_direction(-1), 0u);
}

TEST_F(WindowTest, a_window_without_a_frame_keeps_its_gravity_point_too)
{
    XSizeHints hints = {};
    hints.flags = PWinGravity;
    hints.win_gravity = SouthEastGravity;
    client.set_normal_hints(hints);
    client.map();
    auto window = manage();
    const XWindowAttributes before = client.attrs();

    XConfigureRequestEvent e = {};
    e.window = client.window;
    e.value_mask = CWWidth | CWHeight;
    e.width = before.width + 40;
    e.height = before.height + 30;
    window->configure_request(e);

    EXPECT_EQ(client.attrs().x, before.x - 40);
    EXPECT_EQ(client.attrs().y, before.y - 30);
    EXPECT_EQ(client.attrs().width, before.width + 40);
}

TEST_F(WindowTest, a_requested_size_is_fitted_to_the_hints_of_the_client)
{
    client.set_size_hints(PMinSize | PMaxSize, 50, 50, 300, 300);
    client.map();
    auto window = manage();

    XConfigureRequestEvent e = {};
    e.window = client.window;
    e.value_mask = CWWidth | CWHeight;
    e.width = 900;
    e.height = 10;
    window->configure_request(e);

    EXPECT_EQ(client.attrs().width, 300);
    EXPECT_EQ(client.attrs().height, 50);
}

TEST_F(WindowTest, a_requested_width_alone_keeps_the_height_unless_the_ratio)
{
    client.map();
    auto window = manage();
    const XWindowAttributes before = client.attrs();

    XConfigureRequestEvent e = {};
    e.window = client.window;
    e.value_mask = CWWidth;
    e.width = 333;
    window->configure_request(e);

    EXPECT_EQ(client.attrs().width, 333);
    EXPECT_EQ(client.attrs().height, before.height);
}

TEST_F(WindowTest, a_requested_size_follows_the_ratio_of_the_client)
{
    const XSizeHints hints = aspect_hints(2, 1, 2, 1);
    client.set_normal_hints(hints);
    client.map();
    auto window = manage();

    XConfigureRequestEvent e = {};
    e.window = client.window;
    e.value_mask = CWWidth;
    e.width = 400;
    window->configure_request(e);

    EXPECT_EQ(client.attrs().width, 400);
    EXPECT_EQ(client.attrs().height, 200);
}

TEST_F(WindowTest, the_request_to_move_or_resize_is_passed_on)
{
    auto window = manage();
    Recorder recorder;
    window->on_move_resize_request.connect(recorder.slot());
    window->set_events();

    send(client.window, client_message(client.window, dsp()._NET_WM_MOVERESIZE,
                                       5, 6, tiny::wm::MOVE_BY_POINTER));

    EXPECT_EQ(recorder.calls, 1);
    EXPECT_EQ(recorder.last_event.xclient.data.l[0], 5);
    EXPECT_EQ(recorder.last_event.xclient.data.l[2],
              static_cast<long>(tiny::wm::MOVE_BY_POINTER));
}

// ---- _NET_WM_ALLOWED_ACTIONS ----

namespace {

bool has_action(const std::vector<Atom>& actions, Atom action)
{
    return std::count(actions.begin(), actions.end(), action) == 1;
}

} // namespace

TEST_F(WindowTest, a_window_with_all_functions_allows_all_the_actions)
{
    auto window = manage();

    const auto actions = window->get_allowed_actions();

    for (const Atom action :
         {dsp()._NET_WM_ACTION_MOVE, dsp()._NET_WM_ACTION_RESIZE,
          dsp()._NET_WM_ACTION_MINIMIZE, dsp()._NET_WM_ACTION_STICK,
          dsp()._NET_WM_ACTION_MAXIMIZE_HORZ,
          dsp()._NET_WM_ACTION_MAXIMIZE_VERT, dsp()._NET_WM_ACTION_FULLSCREEN,
          dsp()._NET_WM_ACTION_CHANGE_DESKTOP, dsp()._NET_WM_ACTION_CLOSE,
          dsp()._NET_WM_ACTION_ABOVE, dsp()._NET_WM_ACTION_BELOW}) {
        EXPECT_TRUE(has_action(actions, action));
    }
}

TEST_F(WindowTest, a_window_without_a_frame_cannot_be_rolled_up)
{
    auto window = manage();

    EXPECT_FALSE(has_action(window->get_allowed_actions(),
                            dsp()._NET_WM_ACTION_SHADE));
}

TEST_F(WindowTest, the_functions_of_the_window_limit_the_actions)
{
    using tiny::wm::MotifWMHints;
    auto window = manage(MotifWMHints::FUNC_MOVE | MotifWMHints::FUNC_CLOSE);

    const auto actions = window->get_allowed_actions();

    EXPECT_TRUE(has_action(actions, dsp()._NET_WM_ACTION_MOVE));
    EXPECT_TRUE(has_action(actions, dsp()._NET_WM_ACTION_CLOSE));
    EXPECT_FALSE(has_action(actions, dsp()._NET_WM_ACTION_RESIZE));
    EXPECT_FALSE(has_action(actions, dsp()._NET_WM_ACTION_MINIMIZE));
    EXPECT_FALSE(has_action(actions, dsp()._NET_WM_ACTION_MAXIMIZE_HORZ));
    EXPECT_FALSE(has_action(actions, dsp()._NET_WM_ACTION_MAXIMIZE_VERT));
}

TEST_F(WindowTest, a_dialog_cannot_be_minimized_or_made_fullscreen)
{
    client.set_atoms(dsp()._NET_WM_WINDOW_TYPE,
                     {dsp()._NET_WM_WINDOW_TYPE_DIALOG});
    auto window = manage();

    const auto actions = window->get_allowed_actions();

    EXPECT_FALSE(has_action(actions, dsp()._NET_WM_ACTION_MINIMIZE));
    EXPECT_FALSE(has_action(actions, dsp()._NET_WM_ACTION_FULLSCREEN));
    EXPECT_TRUE(has_action(actions, dsp()._NET_WM_ACTION_CLOSE));
}

TEST_F(WindowTest, the_allowed_actions_are_written_to_the_client)
{
    auto window = manage();

    window->publish_allowed_actions();

    EXPECT_EQ(client.atoms_of(dsp()._NET_WM_ALLOWED_ACTIONS),
              window->get_allowed_actions());
}

// ---- _NET_WM_PING ----

TEST_F(WindowTest, closing_pings_a_client_that_answers_pings)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW, dsp()._NET_WM_PING});
    auto window = manage();

    window->close();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_DELETE_WINDOW));
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp()._NET_WM_PING));
    EXPECT_EQ(e.xclient.data.l[2], static_cast<long>(client.window));
    EXPECT_TRUE(window->has_pending_ping());
}

TEST_F(WindowTest, a_client_without_the_ping_protocol_is_not_pinged)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();

    window->close();

    EXPECT_FALSE(window->has_pending_ping());
}

TEST_F(WindowTest, a_ping_is_sent_once_until_it_is_answered)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();

    window->ping(now);
    window->ping(now + std::chrono::seconds(1));

    EXPECT_EQ(window->get_ping_deadline(), now + WmWindow::ping_timeout);
}

TEST_F(WindowTest, a_window_that_does_not_answer_is_not_responding)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    window->ping(now);

    EXPECT_FALSE(window->expire_ping(now + WmWindow::ping_timeout / 2));
    EXPECT_TRUE(window->is_responding());
    EXPECT_TRUE(window->expire_ping(now + WmWindow::ping_timeout));
    EXPECT_FALSE(window->is_responding());
    EXPECT_FALSE(window->has_pending_ping());
    EXPECT_FALSE(window->expire_ping(now + WmWindow::ping_timeout));
}

TEST_F(WindowTest, the_answer_to_a_ping_makes_the_window_respond_again)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);

    window->pong();

    EXPECT_TRUE(window->is_responding());
    EXPECT_FALSE(window->has_pending_ping());
}

TEST_F(WindowTest, closing_a_window_that_does_not_respond_asks_the_user)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW, dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);
    XEvent e;
    ASSERT_TRUE(client.message(e)); // the ping
    int asked = 0;
    window->on_unresponsive.connect([&](const XEvent&) { ++asked; });

    window->close();

    EXPECT_EQ(asked, 1);
    EXPECT_LE(ask_time(*window), WmWindow::Clock::now());
    EXPECT_FALSE(client.message(e)); // nothing is sent to the client
    const XErrors errors;
    XWindowAttributes attrs;
    EXPECT_NE(XGetWindowAttributes(dsp(), client.window, &attrs), 0);
}

TEST_F(WindowTest, the_user_is_asked_a_while_after_the_ping_is_missed)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    EXPECT_FALSE(window->get_ask_time());
    window->ping(now);

    window->expire_ping(now + WmWindow::ping_timeout);

    EXPECT_EQ(ask_time(*window),
              now + WmWindow::ping_timeout + WmWindow::ask_delay);
    window->pong();
    EXPECT_FALSE(window->get_ask_time());
}

TEST_F(WindowTest, a_second_missed_ping_does_not_postpone_the_question)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);
    const auto asked_at = ask_time(*window);

    window->ping_if_due(now + WmWindow::ping_timeout);
    window->expire_ping(now + 2 * WmWindow::ping_timeout);

    EXPECT_EQ(ask_time(*window), asked_at);
}

TEST_F(WindowTest, nobody_asks_about_a_minimized_window)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);

    window->minimize();

    EXPECT_FALSE(window->get_ask_time());
}

TEST_F(WindowTest, waiting_postpones_the_question)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    window->ping(now);
    window->expire_ping(now + WmWindow::ping_timeout);

    window->postpone_ask(now + WmWindow::ask_delay);

    EXPECT_EQ(ask_time(*window),
              now + WmWindow::ask_delay + WmWindow::wait_delay);
}

TEST_F(WindowTest, a_visible_client_is_pinged_again_and_again)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    XEvent e;

    window->ping_if_due(now);
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp()._NET_WM_PING));
    window->pong();
    window->ping_if_due(now + WmWindow::ping_interval / 2);
    EXPECT_FALSE(client.message(e)); // too early
    window->ping_if_due(now + WmWindow::ping_interval);

    EXPECT_TRUE(client.message(e));
    EXPECT_EQ(ping_wakeup(*window),
              now + WmWindow::ping_interval + WmWindow::ping_timeout);
}

TEST_F(WindowTest, the_next_ping_is_the_wakeup_of_a_client_that_answers)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    const auto now = WmWindow::Clock::now();
    window->ping(now);
    window->pong();

    EXPECT_EQ(ping_wakeup(*window), now + WmWindow::ping_interval);
}

TEST_F(WindowTest, a_client_without_the_ping_protocol_is_never_woken_for)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();
    XEvent e;

    window->ping_if_due(WmWindow::Clock::now());

    EXPECT_FALSE(window->get_ping_wakeup());
    EXPECT_FALSE(client.message(e));
}

TEST_F(WindowTest, a_minimized_window_is_not_pinged)
{
    client.set_protocols({dsp()._NET_WM_PING});
    auto window = manage();
    window->minimize();
    XEvent e;

    window->ping_if_due(WmWindow::Clock::now());

    EXPECT_FALSE(window->get_ping_wakeup());
    EXPECT_FALSE(client.message(e));
}

namespace {

// A process that waits for a signal, `pid` of the client names it
pid_t start_sleeper()
{
    const pid_t pid = fork();
    if (pid == 0) {
        alarm(30); // never outlives a failed test
        pause();
        _exit(0);
    }
    return pid;
}

bool was_killed(pid_t pid)
{
    int status = 0;
    return waitpid(pid, &status, 0) == pid && WIFSIGNALED(status) &&
           WTERMSIG(status) == SIGKILL;
}

void set_machine(FakeClient& client, const std::string& name)
{
    std::string copy = name;
    char* names[] = {copy.data()};
    XTextProperty text;
    ASSERT_NE(XStringListToTextProperty(names, 1, &text), 0);
    XSetWMClientMachine(client.display, client.window, &text);
    XFree(text.value);
    client.sync();
}

} // namespace

TEST_F(WindowTest, killing_a_client_kills_its_process_and_its_connection)
{
    const pid_t pid = start_sleeper();
    ASSERT_GT(pid, 0);
    client.set_cardinals(dsp()._NET_WM_PID, {pid});
    set_machine(client, tiny::wm::local_host_name());
    auto window = manage();
    ASSERT_EQ(window->get_killable_pid(), pid);

    window->kill_client();

    EXPECT_TRUE(was_killed(pid));
    client.abandon(); // the server closes the connection of the client
    const XErrors errors;
    XSync(dsp(), False);
    XWindowAttributes attrs;
    EXPECT_EQ(XGetWindowAttributes(dsp(), client.window, &attrs), 0);
}

TEST_F(WindowTest, a_process_on_another_machine_is_left_alone)
{
    const pid_t pid = start_sleeper();
    ASSERT_GT(pid, 0);
    client.set_cardinals(dsp()._NET_WM_PID, {pid});
    set_machine(client, tiny::wm::local_host_name() + "-elsewhere");
    auto window = manage();
    EXPECT_EQ(window->get_killable_pid(), 0);

    window->kill_client();

    EXPECT_EQ(kill(pid, 0), 0); // still alive
    kill(pid, SIGKILL);
    EXPECT_TRUE(was_killed(pid));
    client.abandon();
}

TEST_F(WindowTest, a_client_without_a_pid_is_killed_by_its_connection)
{
    auto window = manage();
    EXPECT_EQ(window->get_killable_pid(), 0);

    window->kill_client();

    client.abandon();
    const XErrors errors;
    XSync(dsp(), False);
    XWindowAttributes attrs;
    EXPECT_EQ(XGetWindowAttributes(dsp(), client.window, &attrs), 0);
}

// ---- icons ----

TEST_F(WindowTest, wm_hints_icon_of_the_theme_size_is_copied)
{
    client.set_icon_pixmap(tiny::wm::shell_theme.icon, false);

    auto window = manage();

    ASSERT_NE(window->get_icon(), 0u);
    EXPECT_EQ(window->get_icon_mask(), 0u);
}

TEST_F(WindowTest, wm_hints_icon_of_the_theme_size_with_mask)
{
    client.set_icon_pixmap(tiny::wm::shell_theme.icon, true);

    auto window = manage();

    EXPECT_NE(window->get_icon(), 0u);
    EXPECT_NE(window->get_icon_mask(), 0u);
}

TEST_F(WindowTest, wm_hints_icon_of_other_size_is_scaled)
{
    client.set_icon_pixmap(16, true);

    auto window = manage();

    ASSERT_NE(window->get_icon(), 0u);
    ASSERT_NE(window->get_icon_mask(), 0u);
    ::Window root;
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    unsigned int border;
    unsigned int depth;
    XGetGeometry(dsp(), window->get_icon(), &root, &x, &y, &w, &h, &border,
                 &depth);
    EXPECT_EQ(w, tiny::wm::shell_theme.icon);
    EXPECT_EQ(h, tiny::wm::shell_theme.icon);
}

TEST_F(WindowTest, wm_hints_without_icon_leave_no_icon)
{
    XWMHints hints = {};
    hints.flags = InputHint;
    hints.input = True;
    XSetWMHints(client.display, client.window, &hints);
    client.sync();

    EXPECT_EQ(manage()->get_icon(), 0u);
}

TEST_F(WindowTest, wm_hints_with_a_broken_icon_pixmap_leave_no_icon)
{
    XWMHints hints = {};
    hints.flags = IconPixmapHint;
    hints.icon_pixmap = 0x7fffff; // no such pixmap
    {
        const XErrors errors;
        XSetWMHints(client.display, client.window, &hints);
        client.sync();
        EXPECT_EQ(manage()->get_icon(), 0u);
    }
}

TEST_F(WindowTest, net_wm_icon_is_converted_to_a_pixmap)
{
    client.set_net_icon(tiny::wm::shell_theme.icon, tiny::wm::shell_theme.icon);

    auto window = manage();

    EXPECT_NE(window->get_icon(), 0u);
    EXPECT_NE(window->get_icon_mask(), 0u);
}

TEST_F(WindowTest, small_net_wm_icon_is_scaled)
{
    client.set_net_icon(16, 16);

    auto window = manage();

    ASSERT_NE(window->get_icon(), 0u);
    ::Window root;
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    unsigned int border;
    unsigned int depth;
    XGetGeometry(dsp(), window->get_icon(), &root, &x, &y, &w, &h, &border,
                 &depth);
    EXPECT_EQ(w, tiny::wm::shell_theme.icon);
    EXPECT_EQ(h, tiny::wm::shell_theme.icon);
}

TEST_F(WindowTest, the_best_net_wm_icon_is_chosen_from_several)
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

TEST_F(WindowTest, icon_update_replaces_the_icon)
{
    client.set_net_icon(tiny::wm::shell_theme.icon, tiny::wm::shell_theme.icon);
    auto window = manage();
    window->set_events();
    ASSERT_NE(window->get_icon(), 0u);
    const Pixmap first = window->get_icon();

    client.set_net_icon(32, 32);
    send(client.window, property_event(client.window, dsp()._NET_WM_ICON));

    EXPECT_NE(window->get_icon(), 0u);
    EXPECT_NE(window->get_icon(), first);
}

TEST_F(WindowTest, icon_name_is_read)
{
    XSetIconName(client.display, client.window, "short");
    client.sync();
    auto window = manage();
    window->set_events();

    send(client.window, property_event(client.window, dsp().WM_ICON_NAME));

    EXPECT_EQ(window->get_wm_icon_name(), "short");
}

TEST_F(WindowTest, icon_name_falls_back_to_the_plain_wm_icon_name)
{
    // XSetIconName also writes _NET_WM_ICON_NAME, so set only the old property.
    const char name[] = "plain";
    XChangeProperty(client.display, client.window, XA_WM_ICON_NAME, XA_STRING,
                    8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(name), 5);
    client.sync();
    auto window = manage();
    window->set_events();

    send(client.window, property_event(client.window, dsp().WM_ICON_NAME));

    EXPECT_EQ(window->get_wm_icon_name(), "plain");
}

TEST_F(WindowTest, net_wm_icon_name_is_preferred)
{
    client.set_utf8(dsp()._NET_WM_ICON_NAME, "dlouhé");
    auto window = manage();
    window->set_events();
    window->update_properties();

    send(client.window, property_event(client.window, dsp()._NET_WM_ICON_NAME));

    EXPECT_EQ(window->get_wm_icon_name(), "dlouhé");
}

// ---- initial size of a window that starts maximized ----

namespace {

// Opens the protected restore size of the window.
class SizedWindow : public WmWindow {
  public:
    using WmWindow::WmWindow;
    tiny::Rect restore_size() const { return state; }
    long hint_flags() const { return hints->flags; }
};

} // namespace

TEST_F(WindowTest, removed_normal_hints_leave_no_limits)
{
    client.set_size_hints(PMinSize | PMaxSize, 300, 200, 300, 200);
    SizedWindow window(client.window, root_window(), 0xFFFF, screen_area());
    ASSERT_NE(window.hint_flags() & PMinSize, 0);

    XDeleteProperty(client.display, client.window, XA_WM_NORMAL_HINTS);
    client.sync();
    window.update_normal_hints();

    EXPECT_EQ(window.hint_flags(), 0);
}

TEST_F(WindowTest,
       maximized_window_without_size_gets_two_thirds_of_the_work_area)
{
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});

    const SizedWindow window(client.window, root_window(), 0xFFFF,
                             screen_area());

    EXPECT_EQ(window.restore_size().width, screen_area().width / 3 * 2);
    EXPECT_EQ(window.restore_size().height, screen_area().height / 3 * 2);
}

TEST_F(WindowTest, maximized_window_is_not_smaller_than_its_minimum_size)
{
    const int min_w = screen().width;
    const int min_h = screen().height;
    client.set_size_hints(PMinSize, min_w, min_h, 0, 0);
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});

    const SizedWindow window(client.window, root_window(), 0xFFFF,
                             screen_area());

    EXPECT_EQ(window.restore_size().width, min_w);
    EXPECT_EQ(window.restore_size().height, min_h);
}

TEST_F(WindowTest, maximized_window_is_not_bigger_than_its_maximum_size)
{
    client.set_size_hints(PMaxSize, 0, 0, 120, 90);
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});

    const SizedWindow window(client.window, root_window(), 0xFFFF,
                             screen_area());

    EXPECT_EQ(window.restore_size().width, 120);
    EXPECT_EQ(window.restore_size().height, 90);
}

TEST_F(WindowTest, maximized_window_with_aspect_ratio_still_gets_a_size)
{
    client.set_size_hints(PAspect, 0, 0, 0, 0);
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});

    const SizedWindow window(client.window, root_window(), 0xFFFF,
                             screen_area());

    EXPECT_EQ(window.restore_size().width, screen().width / 3 * 2);
}

TEST_F(WindowTest, a_dock_that_comes_later_pushes_a_normal_window_away)
{
    WmWindow window(client.window, root_window(), 0xFFFF, screen_area());
    XMoveWindow(client.display, client.window, 10, 0);
    client.sync();

    window.set_work_area({0, 40, screen_area().width, screen_area().height});

    EXPECT_GE(client.attrs().y, 41);
}

TEST_F(WindowTest, maximized_window_with_aspect_ratio_gets_its_ratio)
{
    XSizeHints hints = {};
    hints.flags = PAspect;
    hints.min_aspect = {2, 1};
    hints.max_aspect = {2, 1};
    client.set_normal_hints(hints);
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});

    const SizedWindow window(client.window, root_window(), 0xFFFF,
                             screen_area());

    EXPECT_EQ(window.restore_size().width, 2 * window.restore_size().height);
}

// ---- create ----

TEST_F(WindowTest, create_maps_and_prepares_the_client)
{
    XWindowAttributes attrs = client.attrs();

    const std::unique_ptr<WmWindow> window(WmWindow::create(
            root_window(), client.window, attrs, 0xFFFF, screen_area()));

    attrs = client.attrs();
    EXPECT_EQ(attrs.map_state, IsViewable);
    EXPECT_EQ(attrs.border_width, 1);
    const long mask = attrs.your_event_mask; // the client's own selection
    (void)mask;
    EXPECT_TRUE(tiny::handlers.count({FocusIn, client.window}));
    EXPECT_TRUE(tiny::handlers.count({PropertyNotify, client.window}));
}

TEST_F(WindowTest, set_events_connects_all_handlers_and_destructor_removes_them)
{
    {
        auto window = manage();
        window->set_events();
        for (const int type :
             {FocusIn, FocusOut, ClientMessage, PropertyNotify, KeyRelease,
              ButtonPress, ButtonRelease, MotionNotify}) {
            EXPECT_TRUE(tiny::handlers.count({type, client.window})) << type;
        }
    }

    for (const int type :
         {FocusIn, FocusOut, ClientMessage, PropertyNotify, KeyRelease,
          ButtonPress, ButtonRelease, MotionNotify}) {
        EXPECT_FALSE(tiny::handlers.count({type, client.window})) << type;
    }
}

TEST_F(WindowTest, set_events_grabs_alt_f4)
{
    auto window = manage();
    window->set_events();
    const XErrors errors;

    // another client can't grab the same key: BadAccess
    XGrabKey(client.display, XKeysymToKeycode(client.display, XK_F4), Mod1Mask,
             client.window, False, GrabModeAsync, GrabModeAsync);
    client.sync();

    EXPECT_EQ(errors.errors(), 1);
}

// ---- states ----

TEST_F(WindowTest, set_minimized_updates_state_and_property)
{
    auto window = manage();

    window->set_minimized(true);
    EXPECT_TRUE(window->get_minimized());
    EXPECT_TRUE(client.state_has(dsp()._NET_WM_STATE_HIDDEN));

    window->set_minimized(false);
    EXPECT_FALSE(window->get_minimized());
    EXPECT_FALSE(client.state_has(dsp()._NET_WM_STATE_HIDDEN));
}

TEST_F(WindowTest, set_maximized_updates_state_and_property)
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

TEST_F(WindowTest, maximize_fills_the_screen_and_restore_returns_the_size)
{
    client.map();
    XMoveWindow(client.display, client.window, 10,
                tiny::wm::shell_theme.panel + 20);
    client.sync();
    auto window = manage();

    window->maximize();

    const XWindowAttributes max = client.attrs();
    EXPECT_EQ(max.x, 0);
    EXPECT_EQ(max.y, tiny::wm::shell_theme.panel);
    EXPECT_EQ(max.width, screen().width);
    EXPECT_EQ(max.height, screen().height - tiny::wm::shell_theme.panel);
    EXPECT_TRUE(window->get_maximized());

    window->restore();

    const XWindowAttributes restored = client.attrs();
    EXPECT_EQ(restored.x, 10);
    EXPECT_EQ(restored.y, tiny::wm::shell_theme.panel + 20);
    EXPECT_EQ(restored.width, 200);
    EXPECT_EQ(restored.height, 100);
    EXPECT_FALSE(window->get_maximized());
}

TEST_F(WindowTest, grow_by_adds_the_extents_on_every_side)
{
    const tiny::Rect grown = tiny::wm::grow_by({10, 20, 100, 50}, {1, 2, 3, 4});

    EXPECT_EQ(grown.x, 9);
    EXPECT_EQ(grown.y, 17);
    EXPECT_EQ(grown.width, 103);
    EXPECT_EQ(grown.height, 57);
}

TEST_F(WindowTest, the_shadow_of_a_gtk_window_lies_outside_of_the_work_area)
{
    client.map();
    client.set_cardinals(dsp()._GTK_FRAME_EXTENTS, {10, 12, 8, 14});
    auto window = manage();

    window->maximize();

    const XWindowAttributes max = client.attrs();
    EXPECT_EQ(max.x, -10);
    EXPECT_EQ(max.y, tiny::wm::shell_theme.panel - 8);
    EXPECT_EQ(max.width, screen().width + 22);
    EXPECT_EQ(max.height,
              screen().height - tiny::wm::shell_theme.panel + 8 + 14);
}

TEST_F(WindowTest, a_gtk_window_that_drops_its_shadow_is_placed_again)
{
    client.map();
    client.set_cardinals(dsp()._GTK_FRAME_EXTENTS, {10, 10, 10, 10});
    auto window = manage();
    window->set_events();
    window->maximize();

    client.set_cardinals(dsp()._GTK_FRAME_EXTENTS, {0, 0, 0, 0});
    send(client.window,
         property_event(client.window, dsp()._GTK_FRAME_EXTENTS));

    const XWindowAttributes max = client.attrs();
    EXPECT_EQ(max.x, 0);
    EXPECT_EQ(max.width, screen().width);
}

TEST_F(WindowTest, wrong_gtk_extents_are_limited_or_ignored)
{
    client.map();
    client.set_cardinals(dsp()._GTK_FRAME_EXTENTS, {5, 5});
    tiny::wm::FrameExtents none =
            WmWindow::get_gtk_frame_extents(client.window);
    EXPECT_EQ(none.left + none.right + none.top + none.bottom, 0);

    client.set_cardinals(dsp()._GTK_FRAME_EXTENTS, {100000, 0, 0, 0});
    EXPECT_EQ(WmWindow::get_gtk_frame_extents(client.window).left, 512);
}

TEST_F(WindowTest, a_fullscreen_window_covers_the_area_it_was_given)
{
    client.map();
    auto window = manage();
    window->set_fullscreen_span({100, 50, 500, 400}); // not fullscreen yet
    ASSERT_EQ(client.attrs().width, 200);

    window->fullscreen(true);

    const XWindowAttributes full = client.attrs();
    EXPECT_EQ(full.x, 100);
    EXPECT_EQ(full.y, 50);
    EXPECT_EQ(full.width, 500);
    EXPECT_EQ(full.height, 400);
}

TEST_F(WindowTest, the_area_of_a_fullscreen_window_can_change_and_be_dropped)
{
    client.map();
    auto window = manage();
    window->fullscreen(true);

    window->set_fullscreen_span({10, 20, 300, 200});
    EXPECT_EQ(client.attrs().width, 300);

    client.set_cardinals(dsp()._NET_WM_FULLSCREEN_MONITORS, {0, 0, 0, 0});
    window->drop_fullscreen_span();
    EXPECT_EQ(client.attrs().width, screen().width);
    EXPECT_TRUE(client.cardinals(dsp()._NET_WM_FULLSCREEN_MONITORS).empty());
    window->drop_fullscreen_span(); // nothing to drop
}

TEST_F(WindowTest, the_area_is_forgotten_when_the_window_leaves_fullscreen)
{
    client.map();
    auto window = manage();
    window->set_fullscreen_span({10, 20, 300, 200});
    window->fullscreen(true);
    client.set_cardinals(dsp()._NET_WM_FULLSCREEN_MONITORS, {0, 0, 0, 0});

    window->fullscreen(false);
    window->fullscreen(true);

    EXPECT_FALSE(window->has_fullscreen_span());
    EXPECT_EQ(client.attrs().width, screen().width);
    EXPECT_TRUE(client.cardinals(dsp()._NET_WM_FULLSCREEN_MONITORS).empty());
}

TEST_F(WindowTest, fullscreen_covers_the_whole_screen_and_back)
{
    client.map();
    auto window = manage();

    window->fullscreen(true);
    const XWindowAttributes full = client.attrs();
    EXPECT_TRUE(window->is_fullscreen());
    EXPECT_EQ(full.x, 0);
    EXPECT_EQ(full.y, 0);
    EXPECT_EQ(full.width, screen().width);
    EXPECT_EQ(full.height, screen().height);

    window->fullscreen(true); // nothing changes
    window->fullscreen(false);
    const XWindowAttributes back = client.attrs();
    EXPECT_FALSE(window->is_fullscreen());
    EXPECT_EQ(back.x, 10);
    EXPECT_EQ(back.y, 20);
    EXPECT_EQ(back.width, 200);
    EXPECT_EQ(back.height, 100);
}

TEST_F(WindowTest, maximize_twice_keeps_the_original_size)
{
    client.map();
    auto window = manage();

    window->maximize();
    window->maximize();
    window->restore();

    EXPECT_EQ(client.attrs().width, 200);
}

TEST_F(WindowTest, restore_of_maximized_window_centers_it_on_the_pointer)
{
    client.map();
    auto window = manage();
    window->maximize();

    window->restore(500, 400);

    const XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(attrs.x, 500 - 200 / 2);
    EXPECT_EQ(attrs.y, 400 - 100 / 2);
}

TEST_F(WindowTest, minimize_hides_the_window_and_restore_shows_it)
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
TEST_F(WindowTest, window_maximized_at_start_gets_a_usable_restore_size)
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

TEST_F(WindowTest, set_focus_focuses_the_client_and_marks_it)
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

TEST_F(WindowTest, return_focus_clears_the_focused_mark)
{
    client.map();
    auto window = manage();
    window->set_focus();

    window->return_focus();

    EXPECT_FALSE(window->is_focused());
    EXPECT_FALSE(client.state_has(dsp()._NET_WM_STATE_FOCUSED));
}

TEST_F(WindowTest, set_focus_restores_a_minimized_window)
{
    client.map();
    auto window = manage();
    window->minimize();

    window->set_focus();

    EXPECT_FALSE(window->get_minimized());
    EXPECT_EQ(client.attrs().map_state, IsViewable);
}

// A client that supports WM_TAKE_FOCUS must be told to take the focus.
TEST_F(WindowTest, set_focus_sends_wm_take_focus)
{
    client.map();
    client.set_protocols({dsp().WM_TAKE_FOCUS});
    auto window = manage();

    window->set_focus();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_TAKE_FOCUS));
}

TEST_F(WindowTest, set_focus_does_not_message_clients_without_take_focus)
{
    client.map();
    auto window = manage();

    window->set_focus();

    XEvent e;
    EXPECT_FALSE(client.message(e));
}

// ---- close ----

TEST_F(WindowTest, close_asks_a_cooperating_client_to_exit)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    auto window = manage();

    window->close();

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.message_type, dsp().WM_PROTOCOLS);
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_DELETE_WINDOW));
}

TEST_F(WindowTest, close_kills_a_client_without_delete_protocol)
{
    auto window = manage();

    window->close();
    XSync(dsp(), False);

    const XErrors errors;
    XWindowAttributes attrs;
    XGetWindowAttributes(dsp(), client.window, &attrs);
    EXPECT_EQ(errors.errors(), 1); // BadWindow: the client is gone
    client.abandon();
}

// ---- events ----

TEST_F(WindowTest, focus_in_signals_on_focus)
{
    auto window = manage();
    window->set_events();
    Recorder recorder;
    window->on_focus.connect(recorder.slot());

    send(client.window, make_event(FocusIn));

    EXPECT_EQ(recorder.calls, 1);
}

TEST_F(WindowTest, focus_in_of_a_grab_or_of_the_pointer_is_not_a_focus_change)
{
    auto window = manage();
    window->set_events();
    Recorder recorder;
    window->on_focus.connect(recorder.slot());

    // what the Alt + Tab grab of the window manager makes the server report
    XEvent grab = make_event(FocusIn);
    grab.xfocus.mode = NotifyGrab;
    grab.xfocus.detail = NotifyPointer;
    send(client.window, grab);
    XEvent ungrab = make_event(FocusIn);
    ungrab.xfocus.mode = NotifyUngrab;
    send(client.window, ungrab);
    XEvent under_pointer = make_event(FocusIn);
    under_pointer.xfocus.detail = NotifyPointer;
    send(client.window, under_pointer);
    XEvent inferior = make_event(FocusIn);
    inferior.xfocus.detail = NotifyInferior;
    send(client.window, inferior);
    EXPECT_EQ(recorder.calls, 0);

    // a change while the keyboard is grabbed is a real one
    XEvent while_grabbed = make_event(FocusIn);
    while_grabbed.xfocus.mode = NotifyWhileGrabbed;
    while_grabbed.xfocus.detail = NotifyNonlinear;
    send(client.window, while_grabbed);
    EXPECT_EQ(recorder.calls, 1);
}

TEST_F(WindowTest, focus_out_is_ignored)
{
    auto window = manage();
    window->set_events();

    send(client.window, make_event(FocusOut));

    EXPECT_FALSE(window->is_focused());
}

TEST_F(WindowTest, client_message_maximizes_and_restores)
{
    client.map();
    auto window = manage();
    window->set_events();

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(tiny::Display::_NET_WM_STATE_ADD),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_VERT),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_HORZ)));
    EXPECT_TRUE(window->get_maximized());

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(tiny::Display::_NET_WM_STATE_REMOVE),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_VERT),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_HORZ)));
    EXPECT_FALSE(window->get_maximized());
}

TEST_F(WindowTest, client_message_maximizes_by_one_atom_or_by_toggle)
{
    client.map();
    auto window = manage();
    window->set_events();
    const auto state = [&](long action, Atom first, Atom second = None) {
        send(client.window, client_message(client.window, dsp()._NET_WM_STATE,
                                           action, static_cast<long>(first),
                                           static_cast<long>(second)));
    };

    state(tiny::Display::_NET_WM_STATE_ADD, dsp()._NET_WM_STATE_MAXIMIZED_HORZ);
    EXPECT_TRUE(window->get_maximized());
    state(tiny::Display::_NET_WM_STATE_TOGGLE,
          dsp()._NET_WM_STATE_MAXIMIZED_VERT,
          dsp()._NET_WM_STATE_MAXIMIZED_HORZ);
    EXPECT_FALSE(window->get_maximized());
    state(tiny::Display::_NET_WM_STATE_TOGGLE,
          dsp()._NET_WM_STATE_MAXIMIZED_VERT,
          dsp()._NET_WM_STATE_MAXIMIZED_HORZ);
    EXPECT_TRUE(window->get_maximized());
}

TEST_F(WindowTest, client_message_changes_both_atoms_of_a_request)
{
    client.map();
    auto window = manage();
    window->set_events();

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(tiny::Display::_NET_WM_STATE_ADD),
                        static_cast<long>(dsp()._NET_WM_STATE_STICKY),
                        static_cast<long>(dsp()._NET_WM_STATE_FULLSCREEN)));

    EXPECT_TRUE(window->is_fullscreen());
}

TEST_F(WindowTest, removing_the_maximized_state_of_a_normal_window_does_nothing)
{
    client.map();
    auto window = manage();
    window->set_events();
    window->minimize();

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(tiny::Display::_NET_WM_STATE_REMOVE),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_VERT),
                        static_cast<long>(dsp()._NET_WM_STATE_MAXIMIZED_HORZ)));

    EXPECT_TRUE(window->get_minimized());
}

TEST_F(WindowTest, client_message_toggles_fullscreen)
{
    client.map();
    auto window = manage();
    window->set_events();
    const auto fullscreen = [&](long action) {
        send(client.window,
             client_message(client.window, dsp()._NET_WM_STATE, action,
                            static_cast<long>(dsp()._NET_WM_STATE_FULLSCREEN)));
    };

    fullscreen(tiny::Display::_NET_WM_STATE_ADD);
    EXPECT_TRUE(window->is_fullscreen());
    fullscreen(tiny::Display::_NET_WM_STATE_REMOVE);
    EXPECT_FALSE(window->is_fullscreen());
    fullscreen(tiny::Display::_NET_WM_STATE_TOGGLE);
    EXPECT_TRUE(window->is_fullscreen());
    fullscreen(tiny::Display::_NET_WM_STATE_TOGGLE);
    EXPECT_FALSE(window->is_fullscreen());
}

TEST_F(WindowTest, client_message_toggles_modal)
{
    client.map();
    auto window = manage();
    window->set_events();
    const auto modal = [&](long action) {
        send(client.window,
             client_message(client.window, dsp()._NET_WM_STATE, action,
                            static_cast<long>(dsp()._NET_WM_STATE_MODAL)));
    };

    modal(tiny::Display::_NET_WM_STATE_ADD);
    EXPECT_TRUE(window->is_modal());
    modal(tiny::Display::_NET_WM_STATE_REMOVE);
    EXPECT_FALSE(window->is_modal());
    modal(tiny::Display::_NET_WM_STATE_TOGGLE);
    EXPECT_TRUE(window->is_modal());
    modal(tiny::Display::_NET_WM_STATE_TOGGLE);
    EXPECT_FALSE(window->is_modal());
}

TEST_F(WindowTest, modal_state_is_published)
{
    client.map();
    auto window = manage();

    window->set_modal(true);

    std::vector<Atom> atoms;
    window->get_wm_states(atoms);
    EXPECT_NE(std::find(atoms.begin(), atoms.end(), dsp()._NET_WM_STATE_MODAL),
              atoms.end());
}

TEST_F(WindowTest, ask_to_close_needs_the_delete_protocol)
{
    client.map();
    auto window = manage();

    EXPECT_FALSE(window->ask_to_close());
}

TEST_F(WindowTest, client_message_with_other_state_changes_is_ignored)
{
    client.map();
    auto window = manage();
    window->set_events();

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(tiny::Display::_NET_WM_STATE_ADD),
                        static_cast<long>(dsp()._NET_WM_STATE_STICKY)));
    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(tiny::Display::_NET_WM_STATE_REMOVE),
                        static_cast<long>(dsp()._NET_WM_STATE_STICKY)));
    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE,
                        static_cast<long>(tiny::Display::_NET_WM_STATE_TOGGLE),
                        static_cast<long>(dsp()._NET_WM_STATE_STICKY)));

    EXPECT_FALSE(window->get_maximized());
}

TEST_F(WindowTest, skip_taskbar_above_below_and_attention_can_be_requested)
{
    client.map();
    auto window = manage();
    window->set_events();
    const auto add = static_cast<long>(tiny::Display::_NET_WM_STATE_ADD);
    const auto remove = static_cast<long>(tiny::Display::_NET_WM_STATE_REMOVE);
    const auto toggle = static_cast<long>(tiny::Display::_NET_WM_STATE_TOGGLE);

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE, add,
                        static_cast<long>(dsp()._NET_WM_STATE_SKIP_TASKBAR),
                        static_cast<long>(dsp()._NET_WM_STATE_ABOVE)));
    EXPECT_TRUE(window->skip_taskbar());
    EXPECT_TRUE(window->is_above());

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE, toggle,
                        static_cast<long>(dsp()._NET_WM_STATE_ABOVE),
                        static_cast<long>(dsp()._NET_WM_STATE_BELOW)));
    EXPECT_FALSE(window->is_above());
    EXPECT_TRUE(window->is_below());

    send(client.window,
         client_message(
                 client.window, dsp()._NET_WM_STATE, add,
                 static_cast<long>(dsp()._NET_WM_STATE_DEMANDS_ATTENTION)));
    EXPECT_TRUE(window->demands_attention());

    send(client.window,
         client_message(client.window, dsp()._NET_WM_STATE, remove,
                        static_cast<long>(dsp()._NET_WM_STATE_SKIP_TASKBAR)));
    EXPECT_FALSE(window->skip_taskbar());
}

TEST_F(WindowTest, a_window_is_in_one_layer)
{
    auto window = manage();

    window->set_above(true);
    window->set_below(true);

    EXPECT_FALSE(window->is_above());
    EXPECT_TRUE(window->is_below());

    window->set_above(true);
    EXPECT_TRUE(window->is_above());
    EXPECT_FALSE(window->is_below());
}

TEST_F(WindowTest, the_layer_and_taskbar_changes_are_reported_only_once)
{
    auto window = manage();
    Recorder layer;
    Recorder taskbar;
    window->on_layer.connect(layer.slot());
    window->on_skip_taskbar.connect(taskbar.slot());

    window->set_above(true);
    window->set_above(true);
    window->set_skip_taskbar(true);
    window->set_skip_taskbar(true);

    EXPECT_EQ(layer.calls, 1);
    EXPECT_EQ(taskbar.calls, 1);
}

TEST_F(WindowTest, focusing_the_window_clears_the_demand_for_attention)
{
    client.map();
    auto window = manage();
    window->set_demands_attention(true);
    ASSERT_TRUE(window->demands_attention());

    window->set_focus();

    EXPECT_FALSE(window->demands_attention());

    window->set_demands_attention(true); // it is looked at already
    EXPECT_FALSE(window->demands_attention());
}

TEST_F(WindowTest, auxiliary_windows_have_no_dock_entry)
{
    client.set_atoms(dsp()._NET_WM_WINDOW_TYPE,
                     {dsp()._NET_WM_WINDOW_TYPE_UTILITY});
    auto window = manage();

    EXPECT_TRUE(window->is_auxiliary());
    EXPECT_TRUE(window->skip_taskbar());
    EXPECT_FALSE(window->is_minimizable());
}

TEST_F(WindowTest, a_window_without_a_frame_has_no_extents)
{
    client.map();
    auto window = manage();
    window->publish_frame_extents();
    client.sync();

    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dsp(), client.window, dsp()._NET_FRAME_EXTENTS,
                                 0, 4, False, XA_CARDINAL, &type, &format,
                                 &count, &left, &data),
              Success);
    ASSERT_EQ(count, 4u);
    const auto* values = reinterpret_cast<const long*>(data);
    EXPECT_EQ(values[0] + values[1] + values[2] + values[3], 0);
    XFree(data);
}

TEST_F(WindowTest, client_message_can_iconify_the_window)
{
    client.map();
    auto window = manage();
    window->set_events();

    send(client.window,
         client_message(client.window, dsp().WM_CHANGE_STATE, IconicState));

    EXPECT_TRUE(window->get_minimized());
    EXPECT_EQ(client.attrs().map_state, IsUnmapped);
}

TEST_F(WindowTest, the_unmap_of_a_minimized_window_is_ours_but_only_once)
{
    client.map();
    auto window = manage();
    EXPECT_FALSE(window->consume_unmap()); // nothing was unmapped by us

    window->minimize();
    window->minimize(); // already minimized, not unmapped twice

    EXPECT_TRUE(window->consume_unmap());
    EXPECT_FALSE(window->consume_unmap()); // the next one is the client's
}

TEST_F(WindowTest, a_client_that_is_unmapped_already_is_not_counted)
{
    auto window = manage(); // never mapped

    window->minimize();

    EXPECT_TRUE(window->get_minimized());
    EXPECT_FALSE(window->consume_unmap());
}

TEST_F(WindowTest, client_message_cannot_iconify_a_window_that_may_not_be)
{
    client.map();
    auto window = manage(MotifWMHints::FUNC_MOVE);
    window->set_events();

    send(client.window,
         client_message(client.window, dsp().WM_CHANGE_STATE, IconicState));

    EXPECT_FALSE(window->get_minimized());
}

TEST_F(WindowTest, minimizing_the_focused_window_lets_go_of_the_focus)
{
    client.map();
    auto window = manage();
    window->set_focus();
    ASSERT_TRUE(window->is_focused());

    window->minimize();

    ::Window focus;
    int revert;
    XGetInputFocus(dsp(), &focus, &revert);
    EXPECT_NE(focus, client.window);
}

TEST_F(WindowTest, unknown_client_message_is_ignored)
{
    auto window = manage();
    window->set_events();

    send(client.window, client_message(client.window, dsp().WM_NAME));
    send(client.window,
         client_message(client.window, dsp().WM_CHANGE_STATE, NormalState));

    EXPECT_FALSE(window->get_minimized());
}

TEST_F(WindowTest, property_notify_updates_the_cached_values)
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

TEST_F(WindowTest, property_notify_updates_net_wm_name)
{
    client.set_utf8(dsp()._NET_WM_NAME, "first");
    auto window = manage();
    window->set_events();

    client.set_utf8(dsp()._NET_WM_NAME, "second");
    send(client.window, property_event(client.window, dsp()._NET_WM_NAME));

    EXPECT_EQ(window->get_wm_name(), "second");
}

// Properties that appear after the window was managed must be noticed too.
TEST_F(WindowTest, property_notify_reads_net_wm_name_set_later)
{
    client.set_title("plain");
    auto window = manage();
    window->set_events();

    client.set_utf8(dsp()._NET_WM_NAME, "later");
    send(client.window, property_event(client.window, dsp()._NET_WM_NAME));

    EXPECT_EQ(window->get_wm_name(), "later");
}

TEST_F(WindowTest, property_notify_reads_net_wm_icon_set_later)
{
    auto window = manage();
    window->set_events();

    client.set_net_icon(24, 24);
    send(client.window, property_event(client.window, dsp()._NET_WM_ICON));

    EXPECT_NE(window->get_icon(), 0u);
}

TEST_F(WindowTest, property_notify_updates_protocols)
{
    auto window = manage();
    window->set_events();
    client.set_protocols({dsp().WM_DELETE_WINDOW});

    send(client.window, property_event(client.window, dsp().WM_PROTOCOLS));
    window->close();

    XEvent e;
    EXPECT_TRUE(client.message(e));
}

TEST_F(WindowTest, property_notify_updates_the_icon)
{
    auto window = manage();
    window->set_events();
    client.set_net_icon(24, 24);
    window->update_properties();

    send(client.window, property_event(client.window, dsp()._NET_WM_ICON));

    EXPECT_NE(window->get_icon(), 0u);
}

TEST_F(WindowTest, property_notify_of_unknown_property_is_ignored)
{
    auto window = manage();
    window->set_events();

    send(client.window, property_event(client.window, dsp().UTF8_STRING));
}

TEST_F(WindowTest, alt_f4_closes_the_window)
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

TEST_F(WindowTest, other_keys_do_not_close_the_window)
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

TEST_F(WindowTest, plain_click_focuses_the_window_without_dragging)
{
    client.map();
    auto window = manage();
    window->set_events();
    Recorder dragged;
    window->on_drag_begin.connect(dragged.slot());
    XEvent e = make_event(ButtonPress);
    e.xbutton.time = CurrentTime;

    send(client.window, e);

    EXPECT_TRUE(window->is_focused());
    EXPECT_EQ(dragged.calls, 0);
}

TEST_F(WindowTest, alt_click_starts_dragging_and_motion_moves_the_window)
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
    const XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(attrs.x, 10 + 30);
    EXPECT_EQ(attrs.y, 20 + 50);

    send(client.window, make_event(ButtonRelease)); // ungrabs the pointer
}

TEST_F(WindowTest, super_click_moves_the_window_like_alt_click)
{
    client.map();
    auto window = manage();
    window->set_events();
    Recorder dragged;
    window->on_modifier_drag.connect(dragged.slot());
    XEvent press = make_event(ButtonPress);
    press.xbutton.state = Mod4Mask;
    press.xbutton.x_root = 100;
    press.xbutton.y_root = 100;
    press.xbutton.time = CurrentTime;

    send(client.window, press);

    XEvent motion = make_event(MotionNotify);
    motion.xbutton.x_root = 130;
    motion.xbutton.y_root = 150;
    send(client.window, motion);
    const XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(dragged.calls, 1);
    EXPECT_EQ(attrs.x, 10 + 30);
    EXPECT_EQ(attrs.y, 20 + 50);

    send(client.window, make_event(ButtonRelease));
}

TEST_F(WindowTest, dragging_a_maximized_window_restores_it_first)
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
    const XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(attrs.width, 200);
    EXPECT_EQ(attrs.x, 300 - 200 / 2);
    send(client.window, make_event(ButtonRelease));
}

TEST_F(WindowTest, icon_update_without_mask_causes_no_x_error)
{
    client.set_icon_pixmap(tiny::wm::shell_theme.icon, true);
    const XErrors errors;
    {
        auto window = manage();
        client.set_icon_pixmap(tiny::wm::shell_theme.icon, false);
        window->update_wm_hints();
    }
    EXPECT_EQ(errors.errors(), 0);
}

// ---- WM_STATE, withdraw and configure ----

TEST_F(WindowTest, create_sets_wm_state_to_normal)
{
    const XWindowAttributes attrs = client.attrs();
    const std::unique_ptr<WmWindow> window(WmWindow::create(
            root_window(), client.window, attrs, 0xFFFF, screen_area()));

    EXPECT_EQ(client.wm_state(), NormalState);
}

TEST_F(WindowTest, hiding_and_showing_changes_wm_state)
{
    auto window = manage();

    window->set_minimized(true);
    EXPECT_EQ(client.wm_state(), IconicState);

    window->set_minimized(false);
    EXPECT_EQ(client.wm_state(), NormalState);
}

TEST_F(WindowTest, withdraw_sets_wm_state_and_removes_the_state_atoms)
{
    auto window = manage();
    window->set_maximized(true); // publishes _NET_WM_STATE
    ASSERT_TRUE(client.has_property(dsp()._NET_WM_STATE));

    window->withdraw();

    EXPECT_EQ(client.wm_state(), WithdrawnState);
    EXPECT_FALSE(client.has_property(dsp()._NET_WM_STATE));
}

TEST_F(WindowTest, configure_request_moves_the_client_and_tells_it_the_place)
{
    auto window = manage();
    client.watch_structure();
    XConfigureRequestEvent e = {};
    e.type = ConfigureRequest;
    e.window = client.window;
    e.value_mask = CWX | CWY | CWWidth | CWHeight;
    e.x = 50;
    e.y = 60;
    e.width = 300;
    e.height = 150;

    window->configure_request(e);

    const XWindowAttributes attrs = client.attrs();
    EXPECT_EQ(attrs.x, 50);
    EXPECT_EQ(attrs.y, 60);
    EXPECT_EQ(attrs.width, 300);
    EXPECT_EQ(attrs.height, 150);
    XConfigureEvent notify = {};
    ASSERT_TRUE(client.synthetic_configure(notify));
    EXPECT_EQ(notify.x, 50);
    EXPECT_EQ(notify.y, 60);
}

TEST_F(WindowTest, the_notify_counts_the_border_in_the_position)
{
    auto window = manage();
    XSetWindowBorderWidth(dsp(), client.window, 1);
    XMoveWindow(client.display, client.window, 50, 60);
    client.sync();
    client.watch_structure();

    window->send_configure_notify();

    XConfigureEvent notify = {};
    ASSERT_TRUE(client.synthetic_configure(notify));
    EXPECT_EQ(notify.x, 50);
    EXPECT_EQ(notify.y, 60);
    EXPECT_EQ(notify.border_width, 1);
}

TEST_F(WindowTest, configure_request_of_a_maximized_window_changes_nothing)
{
    auto window = manage();
    window->maximize();
    const XWindowAttributes before = client.attrs();
    XConfigureRequestEvent e = {};
    e.type = ConfigureRequest;
    e.window = client.window;
    e.value_mask = CWX | CWWidth;
    e.x = 5;
    e.width = 100;

    window->configure_request(e);

    EXPECT_EQ(client.attrs().x, before.x);
    EXPECT_EQ(client.attrs().width, before.width);
}
