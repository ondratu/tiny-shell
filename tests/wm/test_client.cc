#include "fake_client.h"
#include "wm_helpers.h"
#include "wm/shell_theme.h"

#include <memory>

#include "wm/client.h"
#include "wm/symbolic_icons.h"

namespace {

// Client with the members the tests need to look at.
class Probe : public tiny::wm::Client {
  public:
    using tiny::wm::Client::Client;
    using tiny::wm::Client::cls_btn;
    using tiny::wm::Client::header;
    using tiny::wm::Client::max_btn;
    using tiny::wm::Client::min_btn;
    using tiny::wm::Client::on_responding_changed;
    using tiny::wm::Client::shadow;
    using tiny::wm::Client::size_label;

    // What Client::create does, without the WM_STATE property.
    void setup(::Window root_arg, int x, int y)
    {
        realize(root_arg, x, y);
        on_focus_out(XEvent(), nullptr);
        map_all();
    }
};

// Sets the minimum width of a frame for the lifetime of the object.
class FrameMinWidth {
  public:
    explicit FrameMinWidth(int width): old(tiny::wm::shell_theme.win_min_width)
    {
        tiny::wm::shell_theme.win_min_width = width;
    }

    ~FrameMinWidth() { tiny::wm::shell_theme.win_min_width = old; }

  private:
    int old;
};

class ClientTest : public ::testing::Test {
  protected:
    void SetUp() override { client.map(); }

    // Properties of the client must be set before.
    Probe* build(unsigned long functions = 0xFFFF,
                 unsigned long decorations = 0xFFFF)
    {
        const XWindowAttributes a = client.attrs();
        win = std::make_unique<Probe>(client.window, root_window(), a.width,
                                      a.height, functions, decorations,
                                      screen_area());
        win->setup(root_window(), a.x, a.y);
        pump_events();
        return win.get();
    }

    XWindowAttributes frame() { return attributes_of(win->get_window()); }

    XWindowAttributes child() { return client.attrs(); }

    XWindowAttributes screen() { return attributes_of(root_window()); }

    int header_height() const { return tiny::wm::shell_theme.win_header; }

    // Click on a button of the header.
    void click(tiny::Button& button)
    {
        button.on_click(&button, make_event(ButtonRelease));
    }

    // Limits resizing: 50x50 .. 800x600
    void resizable()
    {
        client.set_size_hints(PMinSize | PMaxSize, 50, 50, 800, 600);
    }

    XEvent pointer(int x_root, int y_root, int type = ButtonPress)
    {
        XEvent e = make_event(type);
        e.xbutton.x_root = x_root;
        e.xbutton.y_root = y_root;
        return e;
    }

    HeaderFont header_font;
    WidgetFont widget_font;
    const ShellFonts shell_fonts;
    FakeClient client; // 10,20 200x100
    std::unique_ptr<Probe> win;
};

// ---- create ----

TEST_F(ClientTest, frame_wraps_the_client)
{
    build();

    auto f = frame();
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, 20);
    EXPECT_EQ(f.width, 200);
    EXPECT_EQ(f.height, (100 + header_height()));
    EXPECT_EQ(f.map_state, IsViewable);
}

TEST_F(ClientTest, client_is_reparented_under_the_header)
{
    build();

    auto c = child();
    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, header_height());
    EXPECT_EQ(c.border_width, 0);
    EXPECT_EQ(c.width, 200);
    EXPECT_EQ(c.height, 100);

    auto kids = children_of(win->get_window());
    EXPECT_NE(std::find(kids.begin(), kids.end(), client.window), kids.end());
}

TEST_F(ClientTest, title_comes_from_the_client)
{
    client.set_title("hello");
    build();

    EXPECT_EQ(win->header.get_title(), "hello");
}

TEST_F(ClientTest, header_has_three_buttons)
{
    build();

    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(),
              4u); // + title
}

TEST_F(ClientTest, window_without_maximize_function_has_no_maximize_button)
{
    build(tiny::wm::MotifWMHints::FUNC_RESIZE |
          tiny::wm::MotifWMHints::FUNC_MINIMIZE |
          tiny::wm::MotifWMHints::FUNC_CLOSE);

    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(), 3u);
}

TEST_F(ClientTest, window_without_minimize_function_has_no_minimize_button)
{
    build(tiny::wm::MotifWMHints::FUNC_MAXIMIZE |
          tiny::wm::MotifWMHints::FUNC_CLOSE);

    EXPECT_FALSE(win->is_minimizable());
    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(), 3u);
}

TEST_F(ClientTest, window_without_close_function_has_no_close_button)
{
    build(tiny::wm::MotifWMHints::FUNC_MAXIMIZE |
          tiny::wm::MotifWMHints::FUNC_MINIMIZE);

    EXPECT_FALSE(win->is_closable());
    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(), 3u);
}

TEST_F(ClientTest, decorations_decide_which_buttons_are_drawn)
{
    const XWindowAttributes a = client.attrs();
    win = std::make_unique<Probe>(
            client.window, root_window(), a.width, a.height, 0xFFFF,
            tiny::wm::MotifWMHints::DECOR_TITLE |
                    tiny::wm::MotifWMHints::DECOR_MINIMIZE,
            screen_area());
    win->setup(root_window(), a.x, a.y);
    pump_events();

    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(),
              3u); // close, min
}

TEST_F(ClientTest, a_window_without_the_title_decoration_has_no_title_bar)
{
    using tiny::wm::MotifWMHints;
    build(0xFFFF, MotifWMHints::DECOR_BORDER | MotifWMHints::DECOR_RESIZE);

    EXPECT_EQ(attributes_of(win->header).map_state, IsUnmapped);
    EXPECT_EQ(child().y, 0);
    EXPECT_EQ(frame().height, child().height);
    const tiny::wm::FrameExtents e = win->frame_extents();
    EXPECT_EQ(e.top, win->get_border());
}

TEST_F(ClientTest, a_window_without_the_border_decoration_has_no_border)
{
    using tiny::wm::MotifWMHints;
    build(0xFFFF, MotifWMHints::DECOR_TITLE | MotifWMHints::DECOR_RESIZE);

    EXPECT_EQ(frame().border_width, 0);
    const tiny::wm::FrameExtents e = win->frame_extents();
    EXPECT_EQ(e.left, 0);
    EXPECT_EQ(e.bottom, 0);
    EXPECT_EQ(e.top, header_height());
}

TEST_F(ClientTest, a_window_without_the_resize_decoration_has_no_handles)
{
    using tiny::wm::MotifWMHints;
    build(0xFFFF, MotifWMHints::DECOR_TITLE | MotifWMHints::DECOR_BORDER);

    EXPECT_TRUE(win->is_resizable());
    EXPECT_EQ(attributes_of(win->shadow).map_state, IsUnmapped);
}

TEST_F(ClientTest, the_title_bar_comes_and_goes_with_the_title_decoration)
{
    using tiny::wm::MotifWMHints;
    build();
    const int height = frame().height;
    const int content_y = frame().y + header_height();

    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0,
                     MotifWMHints::DECOR_BORDER);
    pump_events();

    EXPECT_EQ(attributes_of(win->header).map_state, IsUnmapped);
    EXPECT_EQ(frame().height, height - header_height());
    EXPECT_EQ(child().y, 0);
    EXPECT_EQ(frame().y, content_y); // the content did not move

    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0,
                     MotifWMHints::DECOR_ALL);
    pump_events();

    EXPECT_EQ(attributes_of(win->header).map_state, IsViewable);
    EXPECT_EQ(frame().height, height);
    EXPECT_EQ(child().y, header_height());
}

TEST_F(ClientTest, the_border_comes_and_goes_with_the_border_decoration)
{
    using tiny::wm::MotifWMHints;
    build();
    ASSERT_NE(frame().border_width, 0);

    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0,
                     MotifWMHints::DECOR_TITLE);
    pump_events();
    EXPECT_EQ(frame().border_width, 0);

    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0,
                     MotifWMHints::DECOR_ALL);
    pump_events();
    EXPECT_NE(frame().border_width, 0);
}

TEST_F(ClientTest, the_resize_handles_follow_the_resize_decoration)
{
    using tiny::wm::MotifWMHints;
    build();
    ASSERT_EQ(attributes_of(win->shadow).map_state, IsViewable);

    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0,
                     MotifWMHints::DECOR_TITLE | MotifWMHints::DECOR_BORDER);
    pump_events();
    EXPECT_EQ(attributes_of(win->shadow).map_state, IsUnmapped);

    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0,
                     MotifWMHints::DECOR_ALL);
    pump_events();
    EXPECT_EQ(attributes_of(win->shadow).map_state, IsViewable);
}

TEST_F(ClientTest, a_maximized_window_follows_the_title_decoration)
{
    using tiny::wm::MotifWMHints;
    build();
    win->maximize();

    client.set_motif(MotifWMHints::FLAG_DECORATIONS, 0,
                     MotifWMHints::DECOR_BORDER);
    pump_events();

    EXPECT_EQ(child().height, screen_area().height);
    EXPECT_EQ(win->frame_extents().top, 0);
}

TEST_F(ClientTest, the_buttons_follow_a_change_of_the_motif_hints)
{
    build();
    ASSERT_EQ(mapped_children_of(win->header.get_window()).size(), 4u);

    client.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                     tiny::wm::MotifWMHints::FUNC_CLOSE, 0);
    pump_events();

    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(), 2u);
    EXPECT_FALSE(win->is_minimizable());
    EXPECT_FALSE(win->is_maximizable());
}

TEST_F(ClientTest, the_buttons_come_back_when_the_motif_hints_are_gone)
{
    client.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                     tiny::wm::MotifWMHints::FUNC_CLOSE, 0);
    build(tiny::wm::MotifWMHints::FUNC_CLOSE);
    ASSERT_EQ(mapped_children_of(win->header.get_window()).size(), 2u);

    XDeleteProperty(client.display, client.window, dsp()._MOTIF_WM_HINTS);
    client.sync();
    pump_events();

    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(), 4u);
}

TEST_F(ClientTest, a_change_of_the_motif_hints_keeps_a_fixed_size)
{
    client.set_size_hints(PMinSize | PMaxSize, 200, 100, 200, 100);
    build();
    ASSERT_FALSE(win->is_maximizable());

    client.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                     tiny::wm::MotifWMHints::FUNC_ALL, 0);
    pump_events();

    EXPECT_FALSE(win->is_maximizable());
    EXPECT_FALSE(win->is_resizable());
}

TEST_F(ClientTest, the_remaining_buttons_stay_at_the_right_edge)
{
    build();
    client.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                     tiny::wm::MotifWMHints::FUNC_CLOSE, 0);
    pump_events();

    const XWindowAttributes b = attributes_of(win->cls_btn.get_window());
    EXPECT_EQ(b.x + b.width + 2 * b.border_width + win->header.get_padding(),
              win->header.get_width() - 1);
}

TEST_F(ClientTest, a_dialog_has_only_the_close_button)
{
    client.set_atoms(dsp()._NET_WM_WINDOW_TYPE,
                     {dsp()._NET_WM_WINDOW_TYPE_DIALOG});
    build();

    EXPECT_FALSE(win->is_minimizable() && !win->is_auxiliary());
    EXPECT_EQ(mapped_children_of(win->header.get_window()).size(),
              2u); // + title
}

TEST_F(ClientTest, maximizing_a_window_without_the_button_does_not_fail)
{
    client.set_atoms(dsp()._NET_WM_WINDOW_TYPE,
                     {dsp()._NET_WM_WINDOW_TYPE_UTILITY});
    build();

    win->maximize();
    win->restore();
    pump_events();

    EXPECT_FALSE(win->get_maximized());
}

TEST_F(ClientTest, the_frame_extents_are_the_border_and_the_header)
{
    build();

    const tiny::wm::FrameExtents e = win->frame_extents();
    EXPECT_EQ(e.left, win->get_border());
    EXPECT_EQ(e.right, win->get_border());
    EXPECT_EQ(e.bottom, win->get_border());
    EXPECT_EQ(e.top, header_height() + win->get_border());
}

TEST_F(ClientTest, a_maximized_window_has_no_border_only_the_header)
{
    build();

    win->maximize();

    const tiny::wm::FrameExtents e = win->frame_extents();
    EXPECT_EQ(e.left, 0);
    EXPECT_EQ(e.right, 0);
    EXPECT_EQ(e.bottom, 0);
    EXPECT_EQ(e.top, header_height());
}

TEST_F(ClientTest, a_fullscreen_window_has_no_frame_extents)
{
    build();

    win->fullscreen(true);

    const tiny::wm::FrameExtents e = win->frame_extents();
    EXPECT_EQ(e.left + e.right + e.top + e.bottom, 0);
}

TEST_F(ClientTest, the_frame_extents_come_back_after_the_restore)
{
    build();
    const tiny::wm::FrameExtents before = win->frame_extents();

    win->maximize();
    win->restore(0, 0);

    EXPECT_EQ(win->frame_extents().left, before.left);
    EXPECT_EQ(win->frame_extents().top, before.top);
}

TEST_F(ClientTest, the_frame_extents_estimate_matches_the_real_frame)
{
    build();

    const tiny::wm::FrameExtents e = win->frame_extents();
    const tiny::wm::FrameExtents guess =
            tiny::wm::Client::default_frame_extents();
    EXPECT_EQ(guess.left, e.left);
    EXPECT_EQ(guess.top, e.top);
    EXPECT_EQ(guess.right, e.right);
    EXPECT_EQ(guess.bottom, e.bottom);
}

TEST_F(ClientTest, lowering_puts_the_frame_and_its_shadow_at_the_bottom)
{
    build();

    win->lower();
    pump_events();

    const auto stack = children_of(root_window());
    ASSERT_GE(stack.size(), 2u);
    EXPECT_EQ(stack[0], win->shadow.get_window());
    EXPECT_EQ(stack[1], win->get_window());
}

TEST_F(ClientTest, the_buttons_of_the_title_bar_draw_the_icons_they_are_given)
{
    const IconFiles files({"window-close", "window-minimize", "window-maximize",
                           "window-restore"});
    tiny::wm::SymbolicIcons icons(tiny::get_display(), root_window(),
                                  {files.dir()});
    Probe* w = build();

    w->set_icons(&icons);
    for (const tiny::Widget* b : {static_cast<tiny::Widget*>(&w->cls_btn),
                                  static_cast<tiny::Widget*>(&w->min_btn),
                                  static_cast<tiny::Widget*>(&w->max_btn)}) {
        send(*b, Expose);
        const int icon = tiny::wm::shell_theme.win_icon;
        const Image img(b->get_window(), icon, icon);
        EXPECT_EQ(img.at(icon / 2 + 5, icon / 2 + 5),
                  tiny::wm::shell_theme.button.normal_fg);
    }
}

// ---- the window menu ----

TEST_F(ClientTest, the_right_button_on_the_title_bar_asks_for_the_menu)
{
    build();
    Recorder menu;
    win->on_window_menu_request.connect(menu.slot());
    XEvent e = pointer(120, 60, ButtonRelease);
    e.xbutton.button = Button3;

    win->header.get_title_box()->on_context_menu(win->header.get_title_box(),
                                                 e);

    ASSERT_EQ(menu.calls, 1);
    EXPECT_EQ(menu.last_event.xbutton.x_root, 120);
    EXPECT_EQ(menu.last_event.xbutton.y_root, 60);
}

TEST_F(ClientTest, create_centers_a_window_that_has_no_position_of_its_own)
{
    XMoveWindow(dsp(), client.window, 10, 0);
    const XWindowAttributes a = client.attrs();
    const tiny::Rect area = screen_area();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF, area));

    const XWindowAttributes f = attributes_of(
            static_cast<tiny::wm::Client*>(w.get())->get_window());
    const int whole_width = f.width + 2 * f.border_width;
    const int whole_height = f.height + 2 * f.border_width;
    EXPECT_NEAR(f.x + whole_width / 2, area.x + area.width / 2, 2);
    EXPECT_NEAR(f.y + whole_height / 2, area.y + area.height / 2, 2);
    EXPECT_GE(f.y, tiny::wm::shell_theme.panel);
}

namespace {

XSizeHints position_hints(long flags, int gravity = NorthWestGravity)
{
    XSizeHints hints = {};
    hints.flags = flags | PWinGravity;
    hints.win_gravity = gravity;
    return hints;
}

} // namespace

TEST_F(ClientTest, create_places_the_frame_by_the_gravity_of_a_chosen_position)
{
    XMoveWindow(dsp(), client.window, 300, 200);
    client.set_normal_hints(position_hints(USPosition, SouthEastGravity));
    const XWindowAttributes a = client.attrs();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF, screen_area()));

    const XWindowAttributes f = attributes_of(
            static_cast<tiny::wm::Client*>(w.get())->get_window());
    const tiny::wm::FrameExtents e = w->frame_extents();
    EXPECT_EQ(f.x, 300 - (e.left + e.right));
    EXPECT_EQ(f.y, 200 - (e.top + e.bottom));
}

TEST_F(ClientTest, create_places_the_client_itself_with_the_static_gravity)
{
    XMoveWindow(dsp(), client.window, 300, 200);
    client.set_normal_hints(position_hints(PPosition, StaticGravity));
    const XWindowAttributes a = client.attrs();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF, screen_area()));

    ::Window unused;
    int x;
    int y;
    XTranslateCoordinates(dsp(), client.window, root_window(), 0, 0, &x, &y,
                          &unused);
    EXPECT_EQ(x, 300);
    EXPECT_EQ(y, 200);
}

TEST_F(ClientTest, create_centers_a_window_whose_position_nobody_chose)
{
    XMoveWindow(dsp(), client.window, 300, 200);
    client.set_normal_hints(position_hints(0, SouthEastGravity));
    const XWindowAttributes a = client.attrs();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF, screen_area()));

    const XWindowAttributes f = attributes_of(
            static_cast<tiny::wm::Client*>(w.get())->get_window());
    EXPECT_NEAR(f.x + (f.width + 2 * f.border_width) / 2,
                screen_area().x + screen_area().width / 2, 2);
    EXPECT_NEAR(f.y + (f.height + 2 * f.border_width) / 2,
                screen_area().y + screen_area().height / 2, 2);
}

TEST_F(ClientTest, create_keeps_the_position_of_a_window_that_was_there_already)
{
    XMoveWindow(dsp(), client.window, 300, 200);
    client.set_normal_hints(position_hints(USPosition, StaticGravity));
    const XWindowAttributes a = client.attrs();
    const std::unique_ptr<tiny::wm::Window> w(
            tiny::wm::Client::create(root_window(), client.window, a, 0xFFFF,
                                     0xFFFF, screen_area(), false));

    const XWindowAttributes f = attributes_of(
            static_cast<tiny::wm::Client*>(w.get())->get_window());
    EXPECT_EQ(f.x, 300);
    EXPECT_EQ(f.y, 200);
}

TEST_F(ClientTest, create_makes_a_big_window_fit_the_work_area)
{
    XResizeWindow(dsp(), client.window, 3000, 2000);
    XMoveWindow(dsp(), client.window, 500, 500);
    client.sync();
    const XWindowAttributes a = client.attrs();
    const tiny::Rect area = screen_area();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF, area));

    const XWindowAttributes f = attributes_of(
            static_cast<tiny::wm::Client*>(w.get())->get_window());
    EXPECT_GE(f.x - f.border_width, area.x);
    EXPECT_GE(f.y - f.border_width, area.y);
    EXPECT_LE(f.x + f.width + f.border_width, area.x + area.width);
    EXPECT_LE(f.y + f.height + f.border_width, area.y + area.height);
    const XWindowAttributes c = client.attrs();
    EXPECT_EQ(c.width, f.width);
    EXPECT_EQ(c.height, f.height - header_height());
}

TEST_F(ClientTest, create_keeps_a_small_window_in_its_size)
{
    const XWindowAttributes a = client.attrs();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF, screen_area()));

    const XWindowAttributes c = client.attrs();
    EXPECT_EQ(c.width, a.width);
    EXPECT_EQ(c.height, a.height);
}

TEST_F(ClientTest, maximize_covers_the_work_area_only)
{
    build();
    const tiny::Rect area = {40, 60, 800, 500};
    win->set_work_area(area);

    win->maximize();

    const XWindowAttributes f = frame();
    EXPECT_EQ(f.x, area.x);
    EXPECT_EQ(f.y, area.y);
    EXPECT_EQ(f.width, area.width);
    EXPECT_EQ(f.height, area.height);
}

TEST_F(ClientTest, a_maximized_window_follows_a_changed_work_area)
{
    build();
    win->maximize();

    win->set_work_area({0, 30, 900, 600});

    const XWindowAttributes f = frame();
    EXPECT_EQ(f.width, 900);
    EXPECT_EQ(f.height, 600);
    EXPECT_EQ(child().width, 900);
    EXPECT_EQ(child().height, 600 - header_height());
}

TEST_F(ClientTest, a_window_that_starts_maximized_returns_in_its_ratio)
{
    XSizeHints hints = {};
    hints.flags = PAspect;
    hints.min_aspect = {2, 1};
    hints.max_aspect = {2, 1};
    client.set_normal_hints(hints);
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});
    build();
    win->apply_initial_state();
    ASSERT_TRUE(win->get_maximized());

    win->restore(0, 0);

    EXPECT_EQ(child().width, 2 * child().height);
    EXPECT_LE(frame().width, screen_area().width);
    EXPECT_LE(frame().height, screen_area().height);
}

TEST_F(ClientTest, a_window_that_starts_maximized_returns_to_two_thirds)
{
    client.set_state({dsp()._NET_WM_STATE_MAXIMIZED_VERT,
                      dsp()._NET_WM_STATE_MAXIMIZED_HORZ});
    build();
    win->apply_initial_state();

    win->restore(0, 0);

    EXPECT_EQ(frame().width, screen_area().width / 3 * 2);
    EXPECT_EQ(frame().height, screen_area().height / 3 * 2);
}

TEST_F(ClientTest, a_dock_that_comes_later_pushes_a_normal_window_away)
{
    build();
    win->move_to(10, 0);
    const tiny::Rect area = screen_area();

    win->set_work_area({area.x, area.y + 40, area.width, area.height - 40});

    EXPECT_GE(frame().y, area.y + 40 + frame().border_width);
}

TEST_F(ClientTest, a_dock_that_comes_later_shrinks_a_window_that_does_not_fit)
{
    build();
    win->set_work_area({0, 0, 100, 80});

    EXPECT_LE(frame().width, 100);
    EXPECT_LE(frame().height, 80);
    EXPECT_EQ(child().height, frame().height - header_height());
}

TEST_F(ClientTest, restore_after_a_dock_came_keeps_the_window_out_of_it)
{
    build();
    win->move_to(10, 0);
    win->maximize();
    const tiny::Rect area = screen_area();
    win->set_work_area({area.x, area.y + 40, area.width, area.height - 40});

    win->restore(0, 0);

    EXPECT_GE(frame().y, area.y + 40 + frame().border_width);
}

TEST_F(ClientTest, a_new_work_area_leaves_a_window_inside_of_it_where_it_is)
{
    build();
    const XWindowAttributes before = frame();

    win->set_work_area({0, 0, 2000, 2000});

    EXPECT_EQ(frame().x, before.x);
    EXPECT_EQ(frame().y, before.y);
}

TEST_F(ClientTest, a_new_work_area_does_not_move_a_fullscreen_window)
{
    build();
    win->fullscreen(true);

    win->set_work_area({50, 50, 300, 300});

    EXPECT_EQ(frame().x, 0);
    EXPECT_EQ(frame().y, 0);
}

TEST_F(ClientTest, a_new_work_area_does_not_unroll_a_shaded_window)
{
    build();
    win->shade(true);

    win->set_work_area({0, 0, 100, 80});

    EXPECT_EQ(frame().height, header_height());
    EXPECT_LE(frame().width, 100);
}

TEST_F(ClientTest, a_dock_that_comes_later_pushes_a_shaded_window_away)
{
    build();
    win->move_to(10, 0);
    win->shade(true);
    const tiny::Rect area = screen_area();

    win->set_work_area({area.x, area.y + 40, area.width, area.height - 40});

    EXPECT_GE(frame().y, area.y + 40 + frame().border_width);
    EXPECT_EQ(frame().height, header_height());
}

TEST_F(ClientTest, leaving_fullscreen_of_a_maximized_window_uses_the_new_area)
{
    build();
    win->maximize();
    win->fullscreen(true);
    win->set_work_area({60, 30, 900, 600}); // a dock came meanwhile

    win->fullscreen(false);

    const XWindowAttributes f = frame();
    EXPECT_EQ(f.x, 60);
    EXPECT_EQ(f.width, 900);
    EXPECT_TRUE(win->get_maximized());
    EXPECT_FALSE(win->is_fullscreen());
}

TEST_F(ClientTest, a_move_request_stays_out_of_the_docks)
{
    build();
    win->set_work_area({50, 40, 900, 600});
    XConfigureRequestEvent e = {};
    e.type = ConfigureRequest;
    e.window = client.window;
    e.value_mask = CWX | CWY;
    e.x = 0;
    e.y = 0;

    win->configure_request(e);

    EXPECT_EQ(frame().x - frame().border_width, 50);
    EXPECT_EQ(frame().y - frame().border_width, 40);
}

TEST_F(ClientTest, a_normal_window_does_not_move_with_the_work_area)
{
    build();
    const XWindowAttributes before = frame();

    win->set_work_area({0, 100, 300, 300});

    EXPECT_EQ(frame().x, before.x);
    EXPECT_EQ(frame().width, before.width);
}

TEST_F(ClientTest, create_sets_wm_state_of_the_client_to_normal)
{
    const XWindowAttributes a = client.attrs();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF, screen_area()));

    EXPECT_EQ(client.wm_state(), NormalState);
}

TEST_F(ClientTest, minimize_sets_wm_state_to_iconic_and_restore_to_normal)
{
    build();

    win->minimize();
    EXPECT_EQ(client.wm_state(), IconicState);

    win->restore();
    EXPECT_EQ(client.wm_state(), NormalState);
}

TEST_F(ClientTest, resizable_window_has_a_shadow_below_the_frame)
{
    build();

    auto stack = children_of(root_window());
    ASSERT_GE(stack.size(), 2u);
    EXPECT_EQ(stack.back(), win->get_window());
    EXPECT_EQ(stack[stack.size() - 2], win->shadow.get_window());
}

TEST_F(ClientTest, window_without_resize_function_has_no_shadow)
{
    build(tiny::wm::MotifWMHints::FUNC_MOVE);

    EXPECT_FALSE(win->is_resizable());
    auto stack = children_of(root_window());
    EXPECT_EQ(stack.back(), win->get_window());
    EXPECT_EQ(std::find(stack.begin(), stack.end(), win->shadow.get_window()),
              stack.end());
}

// ---- minimize, maximize ----

TEST_F(ClientTest, minimize_button_hides_the_window)
{
    build();

    click(win->min_btn);

    EXPECT_TRUE(win->get_minimized());
    EXPECT_EQ(frame().map_state, IsUnmapped);
}

TEST_F(ClientTest, restore_shows_a_minimized_window)
{
    build();
    click(win->min_btn);

    win->restore();

    EXPECT_FALSE(win->get_minimized());
    EXPECT_EQ(frame().map_state, IsViewable);
}

TEST_F(ClientTest, maximize_button_fills_the_screen_under_the_panel)
{
    build();

    click(win->max_btn);

    auto s = screen();
    auto f = frame();
    EXPECT_TRUE(win->get_maximized());
    const int panel = tiny::wm::shell_theme.panel;
    EXPECT_EQ(f.x, 0);
    EXPECT_EQ(f.y, panel);
    EXPECT_EQ(f.width, s.width);
    EXPECT_EQ(f.height, s.height - panel);
    EXPECT_EQ(child().width, s.width);
    EXPECT_EQ(child().height, s.height - panel - header_height());
}

TEST_F(ClientTest, fullscreen_covers_the_screen_and_the_header)
{
    build();

    win->fullscreen(true);

    auto s = screen();
    auto f = frame();
    EXPECT_TRUE(win->is_fullscreen());
    EXPECT_EQ(f.x, 0);
    EXPECT_EQ(f.y, 0);
    EXPECT_EQ(f.width, s.width);
    EXPECT_EQ(f.height, s.height);
    EXPECT_EQ(child().y, 0);
    EXPECT_EQ(child().width, s.width);
    EXPECT_EQ(child().height, s.height);
}

TEST_F(ClientTest, leaving_fullscreen_returns_the_frame)
{
    build();
    win->fullscreen(true);

    win->fullscreen(false);

    auto f = frame();
    EXPECT_FALSE(win->is_fullscreen());
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, 20);
    EXPECT_EQ(f.width, 200);
    EXPECT_EQ(f.height, 100 + header_height());
    EXPECT_EQ(child().y, header_height());
    EXPECT_EQ(child().height, 100);
}

TEST_F(ClientTest, fullscreen_of_a_maximized_window_returns_to_maximized)
{
    build();
    win->maximize();
    const auto maximized = frame();

    win->fullscreen(true);
    win->fullscreen(false);

    EXPECT_TRUE(win->get_maximized());
    EXPECT_EQ(frame().y, maximized.y);
    EXPECT_EQ(frame().height, maximized.height);
}

TEST_F(ClientTest, fullscreen_twice_changes_nothing)
{
    build();
    win->fullscreen(true);
    win->fullscreen(true);
    win->fullscreen(false);
    win->fullscreen(false);

    EXPECT_EQ(frame().x, 10);
    EXPECT_EQ(frame().width, 200);
}

TEST_F(ClientTest, maximize_button_restores_a_maximized_window)
{
    build();
    win->move_to(10, screen_area().y + 20);
    const int y = frame().y;
    click(win->max_btn);

    click(win->max_btn);

    auto f = frame();
    EXPECT_FALSE(win->get_maximized());
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, y);
    EXPECT_EQ(f.width, 200);
    EXPECT_EQ(f.height, (100 + header_height()));
    EXPECT_EQ(child().width, 200);
    EXPECT_EQ(child().height, 100);
}

TEST_F(ClientTest, maximize_twice_keeps_the_first_restore_state)
{
    build();
    win->maximize();
    pump_events();

    win->maximize();
    win->restore();

    EXPECT_EQ(frame().x, 10);
    EXPECT_EQ(frame().width, 200);
}

TEST_F(ClientTest, restore_can_center_the_window_on_the_pointer)
{
    build();
    win->maximize();

    win->restore(500, 400);

    EXPECT_EQ(frame().x, 500 - 100);
    EXPECT_EQ(frame().y, 400 - (100 + header_height()) / 2);
}

// ---- close ----

TEST_F(ClientTest, close_button_asks_the_client_to_exit)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    build();

    click(win->cls_btn);

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_DELETE_WINDOW));
}

TEST_F(ClientTest, close_button_kills_an_uncooperative_client)
{
    build();

    click(win->cls_btn);
    XSync(dsp(), False);

    const XErrors errors;
    XWindowAttributes attrs;
    XGetWindowAttributes(dsp(), client.window, &attrs);
    EXPECT_EQ(errors.errors(), 1);
    client.abandon();
}

// ---- focus ----

TEST_F(ClientTest, set_focus_raises_the_window)
{
    build();
    FakeClient other(100, 100);
    other.map();
    const XWindowAttributes a = other.attrs();
    Probe second(other.window, root_window(), a.width, a.height, 0xFFFF, 0xFFFF,
                 screen_area());
    second.setup(root_window(), a.x, a.y);
    ASSERT_EQ(children_of(root_window()).back(), second.get_window());

    win->set_focus();

    auto stack = children_of(root_window());
    EXPECT_EQ(stack.back(), win->get_window());
    EXPECT_EQ(stack[stack.size() - 2], win->shadow.get_window());
    EXPECT_TRUE(win->is_focused());
}

TEST_F(ClientTest, focus_events_enable_and_disable_the_header)
{
    client.set_title("title");
    build();
    // only the text color differs
    ASSERT_NE(tiny::wm::shell_theme.header.get_fg(
                      static_cast<uint8_t>(tiny::Style::State::Normal)),
              tiny::wm::shell_theme.header.get_fg(
                      static_cast<uint8_t>(tiny::Style::State::Disabled)));
    auto header_image = [this] {
        send(win->header, Expose);
        return Image(win->header.get_window(), win->header.get_width(),
                     win->header.get_height());
    };
    const Image disabled = header_image();

    send(client.window, make_event(FocusIn));
    const Image enabled = header_image();
    EXPECT_FALSE(enabled == disabled);

    send(client.window, make_event(FocusOut));
    EXPECT_TRUE(header_image() == disabled);
}

TEST_F(ClientTest, a_grab_does_not_change_the_look_of_the_header)
{
    build();
    send(client.window, make_event(FocusIn));
    send(win->header, Expose);
    const Image focused(win->header.get_window(), win->header.get_width(),
                        win->header.get_height());

    XEvent out = make_event(FocusOut);
    out.xfocus.mode = NotifyGrab;
    send(client.window, out);
    send(win->header, Expose);

    EXPECT_TRUE(focused == Image(win->header.get_window(),
                                 win->header.get_width(),
                                 win->header.get_height()));
}

TEST_F(ClientTest, focus_in_signals_on_focus)
{
    build();
    Recorder r;
    win->on_focus.connect(r.slot());

    send(client.window, make_event(FocusIn));

    EXPECT_EQ(r.calls, 1);
}

// ---- properties ----

TEST_F(ClientTest, title_follows_wm_name)
{
    build();
    client.set_title("changed");
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp().WM_NAME;

    send(client.window, e);

    EXPECT_EQ(win->header.get_title(), "changed");
}

TEST_F(ClientTest, title_follows_net_wm_name)
{
    build();
    client.set_utf8(dsp()._NET_WM_NAME, "utf8 title");
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp()._NET_WM_NAME;

    send(client.window, e);

    EXPECT_EQ(win->header.get_title(), "utf8 title");
}

TEST_F(ClientTest, the_client_cannot_move_itself_inside_the_frame)
{
    build();

    XMoveWindow(client.display, client.window, 30, 40);
    client.sync();

    EXPECT_EQ(child().x, 0);
    EXPECT_EQ(child().y, header_height());
}

TEST_F(ClientTest, normal_hints_change_of_a_gone_client_is_survived)
{
    build();
    const XErrors errors;
    XDestroyWindow(client.display, client.window);
    client.sync();
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp().WM_NORMAL_HINTS;

    send(client.window, e);
}

// ---- move ----

TEST_F(ClientTest, dragging_the_title_moves_the_window)
{
    build();
    auto* title = win->header.get_title_box();

    title->on_drag_begin(title, pointer(100, 100));
    title->on_drag_motion(title, pointer(130, 140, MotionNotify));

    EXPECT_EQ(frame().x, 40);
    EXPECT_EQ(frame().y, 60);
    EXPECT_TRUE(win->is_focused());
}

TEST_F(ClientTest, dragging_the_title_moves_the_shadow_too)
{
    build();
    auto* title = win->header.get_title_box();
    const int before = attributes_of(win->shadow).x;

    title->on_drag_begin(title, pointer(100, 100));
    title->on_drag_motion(title, pointer(130, 140, MotionNotify));

    EXPECT_EQ(attributes_of(win->shadow).x, before + 30);
}

TEST_F(ClientTest, dragging_a_maximized_window_restores_it)
{
    build();
    win->maximize();
    auto* title = win->header.get_title_box();
    XEvent e = pointer(500, 10);
    e.xmotion.x = 500;
    e.xmotion.y = 10;

    title->on_drag_begin(title, e);

    EXPECT_FALSE(win->get_maximized());
    EXPECT_EQ(frame().width, 200);
}

// ---- shade ----

TEST_F(ClientTest, a_shaded_window_shows_only_the_title_bar)
{
    build();
    const int height = frame().height;

    win->shade(true);

    EXPECT_TRUE(win->is_shaded());
    EXPECT_EQ(frame().height, header_height());
    EXPECT_EQ(attributes_of(win->shadow).map_state, IsUnmapped);

    win->shade(false);

    EXPECT_FALSE(win->is_shaded());
    EXPECT_EQ(frame().height, height);
    EXPECT_EQ(attributes_of(win->shadow).map_state, IsViewable);
}

TEST_F(ClientTest, the_shaded_state_is_published)
{
    build();

    win->shade(true);

    std::vector<Atom> atoms;
    win->get_wm_states(atoms);
    EXPECT_EQ(
            std::count(atoms.begin(), atoms.end(), dsp()._NET_WM_STATE_SHADED),
            1);
}

TEST_F(ClientTest, a_double_click_on_the_title_rolls_the_window_up_and_down)
{
    build();
    auto* title = win->header.get_title_box();
    XEvent first = pointer(100, 100);
    first.xbutton.time = 1000;
    XEvent second = pointer(100, 100);
    second.xbutton.time = 1200;

    title->on_drag_begin(title, first);
    EXPECT_FALSE(win->is_shaded());
    title->on_drag_begin(title, second);
    EXPECT_TRUE(win->is_shaded());

    first.xbutton.time = 5000;
    second.xbutton.time = 5100;
    title->on_drag_begin(title, first);
    title->on_drag_begin(title, second);
    EXPECT_FALSE(win->is_shaded());
}

TEST_F(ClientTest, two_slow_clicks_do_not_roll_the_window_up)
{
    build();
    auto* title = win->header.get_title_box();
    XEvent first = pointer(100, 100);
    first.xbutton.time = 1000;
    XEvent second = pointer(100, 100);
    second.xbutton.time = 3000;

    title->on_drag_begin(title, first);
    title->on_drag_begin(title, second);

    EXPECT_FALSE(win->is_shaded());
}

TEST_F(ClientTest, a_window_without_a_title_cannot_be_shaded)
{
    build(0xFFFF, tiny::wm::MotifWMHints::DECOR_BORDER);

    win->shade(true);

    EXPECT_FALSE(win->is_shaded());
}

TEST_F(ClientTest, maximizing_unrolls_a_shaded_window)
{
    build();
    win->shade(true);

    win->maximize();

    EXPECT_FALSE(win->is_shaded());
    EXPECT_TRUE(win->get_maximized());
}

TEST_F(ClientTest, a_shaded_window_ignores_the_resize_of_its_client)
{
    build();
    win->shade(true);
    XEvent e = make_event(ConfigureRequest);
    e.xconfigurerequest.value_mask = CWWidth | CWHeight;
    e.xconfigurerequest.width = 400;
    e.xconfigurerequest.height = 300;

    win->configure_request(e.xconfigurerequest);

    EXPECT_EQ(frame().height, header_height());
}

TEST_F(ClientTest, leaving_fullscreen_keeps_the_handles_the_hints_forbid)
{
    using tiny::wm::MotifWMHints;
    build(0xFFFF, MotifWMHints::DECOR_TITLE | MotifWMHints::DECOR_BORDER);

    win->fullscreen(true);
    win->fullscreen(false);

    EXPECT_EQ(attributes_of(win->shadow).map_state, IsUnmapped);
}

TEST_F(ClientTest, a_hints_change_while_shaded_unrolls_the_window)
{
    build();
    win->shade(true);

    client.set_motif(tiny::wm::MotifWMHints::FLAG_FUNCTIONS,
                     tiny::wm::MotifWMHints::FUNC_ALL, 0);
    pump_events();

    EXPECT_FALSE(win->is_shaded());
    EXPECT_EQ(attributes_of(win->shadow).map_state, IsViewable);
}

TEST_F(ClientTest, a_state_request_shades_the_window)
{
    build();
    XEvent e = make_event(ClientMessage);
    e.xclient.message_type = dsp()._NET_WM_STATE;
    e.xclient.format = 32;
    e.xclient.data.l[0] = tiny::Display::_NET_WM_STATE_ADD;
    e.xclient.data.l[1] = static_cast<long>(dsp()._NET_WM_STATE_SHADED);

    send(client.window, e);
    EXPECT_TRUE(win->is_shaded());

    e.xclient.data.l[0] = tiny::Display::_NET_WM_STATE_TOGGLE;
    send(client.window, e);
    EXPECT_FALSE(win->is_shaded());
}

// ---- resize ----

TEST_F(ClientTest, resizing_the_right_bottom_corner_grows_the_window)
{
    resizable();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(350, 330, MotionNotify), mask);

    pump_events(); // ConfigureNotify updates the widget sizes
    auto f = frame();
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, 20);
    EXPECT_EQ(f.width, 250);
    EXPECT_EQ(f.height, (100 + header_height()) + 30);
    EXPECT_EQ(child().width, 250);
    EXPECT_EQ(child().height, 130);
    EXPECT_EQ(win->header.get_width(), 250u);
}

TEST_F(ClientTest, a_resize_waits_for_the_client_to_draw_the_last_size)
{
    const XSyncCounter counter = client.set_sync_counter();
    resizable();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(350, 330, MotionNotify), mask);
    pump_events();
    EXPECT_EQ(frame().width, 250); // the first size goes at once

    // the client has not drawn it: this one is kept back
    win->on_move_resize_motion(nullptr, pointer(400, 330, MotionNotify), mask);
    pump_events();
    EXPECT_EQ(frame().width, 250);

    client.set_counter(counter, 1); // drawn
    win->on_move_resize_motion(nullptr, pointer(410, 330, MotionNotify), mask);
    pump_events();
    EXPECT_EQ(frame().width, 310);
}

TEST_F(ClientTest, the_last_size_is_used_when_the_resize_ends)
{
    client.set_sync_counter();
    resizable();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(350, 330, MotionNotify), mask);
    win->on_move_resize_motion(nullptr, pointer(400, 330, MotionNotify), mask);
    ASSERT_EQ(frame().width, 250);

    win->on_move_resize_end(nullptr, pointer(400, 330, ButtonRelease), nullptr);
    pump_events();

    EXPECT_EQ(frame().width, 300);
}

TEST_F(ClientTest, a_new_resize_does_not_wait_for_the_one_before)
{
    client.set_sync_counter();
    resizable();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));
    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(350, 330, MotionNotify), mask);
    win->on_move_resize_end(nullptr, pointer(350, 330, ButtonRelease), nullptr);

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(360, 330, MotionNotify), mask);
    pump_events();

    EXPECT_EQ(frame().width, 310); // from the 250 of the first resize
}

TEST_F(ClientTest, resizing_the_left_top_corner_moves_the_window)
{
    resizable();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Left | Position::Top));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(280, 270, MotionNotify), mask);

    auto f = frame();
    EXPECT_EQ(f.x, -10);
    EXPECT_EQ(f.y, -10);
    EXPECT_EQ(f.width, 220);
    EXPECT_EQ(f.height, (100 + header_height()) + 30);
}

TEST_F(ClientTest, resizing_is_limited_by_the_size_hints)
{
    resizable();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(5000, 5000, MotionNotify),
                               mask);
    EXPECT_EQ(frame().width, 800);
    EXPECT_EQ(frame().height,
              600 + header_height()); // limits are of the client

    win->on_move_resize_motion(nullptr, pointer(-5000, -5000, MotionNotify),
                               mask);
    EXPECT_GE(frame().width, tiny::wm::shell_theme.win_min_width);
    EXPECT_GE(frame().height, tiny::wm::shell_theme.win_min_height);
}

TEST_F(ClientTest, resizing_snaps_to_the_resize_increment)
{
    XSizeHints hints = {};
    hints.flags = PMinSize | PMaxSize | PResizeInc;
    hints.min_width = hints.min_height = 50;
    hints.max_width = hints.max_height = 800;
    hints.width_inc = 10;
    hints.height_inc = 10;
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(static_cast<size_t>(Position::Right));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(391, 300, MotionNotify), mask);

    EXPECT_EQ(frame().width, 290);
}

TEST_F(ClientTest, window_without_size_hints_can_be_resized)
{
    // like kitty: no limits, the old code held the size of the start
    client.set_size_hints(PWinGravity, 0, 0, 0, 0);
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(400, 380, MotionNotify), mask);
    EXPECT_EQ(frame().width, 300);
    EXPECT_EQ(frame().height, 100 + header_height() + 80);

    win->on_move_resize_motion(nullptr, pointer(250, 250, MotionNotify), mask);
    EXPECT_EQ(frame().width, 150);
    EXPECT_EQ(child().height, 50);
}

TEST_F(ClientTest, resizing_snaps_to_the_grid_of_the_base_size)
{
    // the client is 200x100, not on the grid 3 + n * 16, 5 + n * 30
    XSizeHints hints = {};
    hints.flags = PMinSize | PBaseSize | PResizeInc;
    hints.base_width = 3;
    hints.base_height = 5;
    hints.min_width = 3 + 2 * 16;
    hints.min_height = 5 + 2 * 30;
    hints.width_inc = 16;
    hints.height_inc = 30;
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(300, 300, MotionNotify), mask);

    EXPECT_EQ((child().width - 3) % 16, 0);
    EXPECT_EQ((child().height - 5) % 30, 0);
    EXPECT_EQ(child().width, 3 + 12 * 16); // 195
    EXPECT_EQ(child().height, 5 + 3 * 30); // 95
}

TEST_F(ClientTest, minimum_of_the_window_manager_keeps_the_grid)
{
    // the grid 3 + n * 7 has no size equal to the minimum width of the frame
    XSizeHints hints = {};
    hints.flags = PBaseSize | PResizeInc;
    hints.base_width = 3;
    hints.width_inc = 7;
    const FrameMinWidth min_width(12);
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(static_cast<size_t>(Position::Right));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(0, 300, MotionNotify), mask);

    EXPECT_GE(child().width, 12);
    EXPECT_EQ((child().width - 3) % 7, 0);
    EXPECT_EQ(child().width, 17);
}

TEST_F(ClientTest,
       maximum_of_the_client_wins_over_the_minimum_of_the_window_manager)
{
    XSizeHints hints = {};
    hints.flags = PMaxSize;
    hints.max_width = 6;
    hints.max_height = 600;
    const FrameMinWidth min_width(12);
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(static_cast<size_t>(Position::Right));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(0, 300, MotionNotify), mask);

    EXPECT_EQ(child().width, 6);
}

TEST_F(ClientTest, resizing_the_left_edge_keeps_the_right_edge_on_the_grid)
{
    XSizeHints hints = {};
    hints.flags = PMinSize | PResizeInc;
    hints.min_width = 40;
    hints.min_height = 40;
    hints.width_inc = 30;
    hints.height_inc = 30;
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(static_cast<size_t>(Position::Left));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(260, 300, MotionNotify), mask);

    // 200 + 40 = 240 pixels, the grid is 40 + n * 30: 220
    EXPECT_EQ(frame().width, 220);
    EXPECT_EQ(frame().x + frame().width, 10 + 200);
}

TEST_F(ClientTest, resizing_a_terminal_shows_its_size_in_units)
{
    XSizeHints hints = {};
    hints.flags = PMinSize | PBaseSize | PResizeInc;
    hints.base_width = 0;
    hints.base_height = 0;
    hints.min_width = 20;
    hints.min_height = 20;
    hints.width_inc = 10;
    hints.height_inc = 20;
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();
    using tiny::Position;
    void* mask = reinterpret_cast<void*>(
            static_cast<size_t>(Position::Right | Position::Bottom));

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    ASSERT_NE(win->size_label, nullptr);
    EXPECT_EQ(win->size_label->get_text(), "20 × 5");

    win->on_move_resize_motion(nullptr, pointer(340, 360, MotionNotify), mask);
    EXPECT_EQ(win->size_label->get_text(), "24 × 8");

    win->on_move_resize_end(nullptr, pointer(340, 360, ButtonRelease), nullptr);
    EXPECT_EQ(win->size_label, nullptr);
}

TEST_F(ClientTest, resizing_a_window_in_pixels_shows_no_size)
{
    resizable();
    build();

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);

    EXPECT_EQ(win->size_label, nullptr);
}

TEST_F(ClientTest, size_label_is_a_small_box_in_the_center_of_the_client)
{
    XSizeHints hints = {};
    hints.flags = PMinSize | PResizeInc;
    hints.min_width = 20;
    hints.min_height = 20;
    hints.width_inc = 10;
    hints.height_inc = 10;
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    pump_events();
    ASSERT_NE(win->size_label, nullptr);
    const XWindowAttributes label =
            attributes_of(win->size_label->get_window());

    EXPECT_LT(label.width, 200); // the client stays visible around it
    EXPECT_LT(label.height, 100);
    EXPECT_NEAR(label.x + label.width / 2.0, 100, 1);
    EXPECT_NEAR(label.y + label.height / 2.0, header_height() + 50, 1);
    EXPECT_EQ(children_of(win->get_window()).back(),
              win->size_label->get_window()); // above the client
}

TEST_F(ClientTest, resizing_unmaximizes_the_window)
{
    resizable();
    build();
    win->maximize();

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);

    EXPECT_FALSE(win->get_maximized());
}

// ---- life ----

TEST_F(ClientTest, destructor_disconnects_handlers_and_destroys_the_frame)
{
    build();
    const ::Window f = win->get_window();
    const ::Window kid = client.window;
    ASSERT_TRUE(tiny::handlers.count({FocusIn, kid}));

    win.reset();

    for (const int type : {FocusIn, FocusOut, PropertyNotify}) {
        EXPECT_FALSE(tiny::handlers.count({type, kid})) << type;
    }
    const XErrors errors;
    XWindowAttributes attrs;
    XGetWindowAttributes(dsp(), f, &attrs);
    EXPECT_EQ(errors.errors(), 1); // BadWindow
}

TEST_F(ClientTest, destructor_gives_the_client_back_to_the_root)
{
    build();

    win.reset();

    ::Window root;
    ::Window parent;
    ::Window* kids = nullptr;
    unsigned int count;
    ASSERT_TRUE(
            XQueryTree(dsp(), client.window, &root, &parent, &kids, &count));
    if (kids) {
        XFree(kids);
    }
    EXPECT_EQ(parent, root_window());
    auto c = child();
    EXPECT_EQ(c.x, 10);
    EXPECT_EQ(c.y, 20 + header_height());
    EXPECT_EQ(c.width, 200);
}

TEST_F(ClientTest, destructor_survives_a_client_that_is_gone)
{
    build();
    XDestroyWindow(client.display, client.window);
    client.sync();

    const XErrors errors;
    win.reset();

    EXPECT_EQ(errors.errors(), 0);
}

// ---- configure request ----

XConfigureRequestEvent configure_request(::Window window, unsigned long mask,
                                         int x, int y, int width, int height)
{
    XConfigureRequestEvent e = {};
    e.type = ConfigureRequest;
    e.window = window;
    e.value_mask = mask;
    e.x = x;
    e.y = y;
    e.width = width;
    e.height = height;
    return e;
}

TEST_F(ClientTest, configure_request_moves_and_resizes_the_frame)
{
    build();

    win->configure_request(configure_request(
            client.window, CWX | CWY | CWWidth | CWHeight, 30, 100, 320, 210));

    EXPECT_EQ(frame().x, 30 - frame().border_width);
    EXPECT_EQ(frame().y, 100 - header_height() - frame().border_width);
    EXPECT_EQ(frame().width, 320);
    EXPECT_EQ(frame().height, 210 + header_height());
    EXPECT_EQ(child().width, 320);
    EXPECT_EQ(child().height, 210);
    EXPECT_EQ(child().x, 0);
    EXPECT_EQ(child().y, header_height());
    EXPECT_EQ(attributes_of(win->header).width, 320);
    EXPECT_EQ(attributes_of(win->shadow).x,
              frame().x - tiny::wm::shell_theme.win_border);
    EXPECT_EQ(attributes_of(win->shadow).height,
              210 + header_height() + 2 * tiny::wm::shell_theme.win_border);
}

TEST_F(ClientTest, configure_request_to_raise_raises_the_frame)
{
    build();
    FakeClient other;
    other.map();
    XRaiseWindow(dsp(), other.window);
    XConfigureRequestEvent e =
            configure_request(client.window, CWStackMode, 0, 0, 0, 0);
    e.detail = Above;

    win->configure_request(e);

    const auto stack = children_of(root_window());
    EXPECT_EQ(stack.back(), win->get_window());
}

TEST_F(ClientTest, configure_request_keeps_the_header_under_the_panel)
{
    build();

    win->configure_request(configure_request(client.window, CWY, 0, 0, 0, 0));

    EXPECT_EQ(frame().y, tiny::wm::shell_theme.panel + frame().border_width);
}

TEST_F(ClientTest, configure_request_keeps_what_it_does_not_ask_for)
{
    build();
    const XWindowAttributes before = frame();

    win->configure_request(
            configure_request(client.window, CWWidth, 0, 0, 333, 0));

    EXPECT_EQ(frame().x, before.x);
    EXPECT_EQ(frame().y, before.y);
    EXPECT_EQ(frame().width, 333);
    EXPECT_EQ(frame().height, before.height);
}

TEST_F(ClientTest,
       configure_request_follows_the_limits_and_the_grid_of_the_hints)
{
    XSizeHints hints = {};
    hints.flags = PMaxSize | PResizeInc;
    hints.max_width = 300;
    hints.max_height = 200;
    hints.width_inc = 10;
    hints.height_inc = 10;
    client.set_normal_hints(hints);
    build();

    win->configure_request(configure_request(client.window, CWWidth | CWHeight,
                                             0, 0, 999, 123));

    EXPECT_EQ(child().width, 300);
    EXPECT_EQ(child().height, 120);
    EXPECT_EQ(frame().height, 120 + header_height());
}

TEST_F(ClientTest, configure_request_follows_the_ratio_of_the_hints)
{
    XSizeHints hints = {};
    hints.flags = PAspect;
    hints.min_aspect.x = 2;
    hints.min_aspect.y = 1;
    hints.max_aspect.x = 2;
    hints.max_aspect.y = 1;
    client.set_normal_hints(hints);
    build();

    win->configure_request(
            configure_request(client.window, CWHeight, 0, 0, 0, 150));

    EXPECT_EQ(child().height, 150);
    EXPECT_EQ(child().width, 300);
}

TEST_F(ClientTest, a_window_that_grows_keeps_its_gravity_point)
{
    client.set_normal_hints(position_hints(0, SouthEastGravity));
    build();
    win->configure_request(
            configure_request(client.window, CWX | CWY, 300, 300, 0, 0));
    const XWindowAttributes before = frame();

    win->configure_request(configure_request(client.window, CWWidth | CWHeight,
                                             0, 0, child().width + 40,
                                             child().height + 30));

    EXPECT_EQ(frame().x, before.x - 40);
    EXPECT_EQ(frame().y, before.y - 30);
}

TEST_F(ClientTest, a_window_that_grows_west_stays_inside_the_work_area)
{
    client.set_normal_hints(position_hints(0, EastGravity));
    build();
    win->configure_request(configure_request(client.window, CWX | CWY, 0,
                                             screen_area().y + 100, 0, 0));
    win->configure_request(configure_request(client.window, CWWidth, 0, 0,
                                             child().width + 80, 0));

    EXPECT_GE(frame().x, screen_area().x + frame().border_width);
}

TEST_F(ClientTest, a_window_with_north_west_gravity_grows_to_the_south_east)
{
    build();
    win->configure_request(
            configure_request(client.window, CWX | CWY, 300, 300, 0, 0));
    const XWindowAttributes before = frame();

    win->configure_request(configure_request(client.window, CWWidth | CWHeight,
                                             0, 0, child().width + 40,
                                             child().height + 30));

    EXPECT_EQ(frame().x, before.x);
    EXPECT_EQ(frame().y, before.y);
}

TEST_F(ClientTest, configure_request_tells_the_client_where_it_is)
{
    build();
    client.watch_structure();

    win->configure_request(
            configure_request(client.window, CWX | CWY, 30, 100, 0, 0));

    XConfigureEvent notify = {};
    ASSERT_TRUE(client.synthetic_configure(notify));
    EXPECT_EQ(notify.x, 30); // where the client asked for its area
    EXPECT_EQ(notify.y, 100);
    EXPECT_EQ(notify.width, child().width);
    EXPECT_EQ(notify.height, child().height);
}

TEST_F(ClientTest, configure_request_of_a_maximized_window_keeps_its_place)
{
    build();
    win->maximize();
    const XWindowAttributes before = frame();

    win->configure_request(configure_request(
            client.window, CWX | CWY | CWWidth | CWHeight, 30, 40, 320, 210));

    EXPECT_EQ(frame().x, before.x);
    EXPECT_EQ(frame().y, before.y);
    EXPECT_EQ(frame().width, before.width);
    EXPECT_EQ(frame().height, before.height);
}

TEST_F(ClientTest, configure_request_of_a_fullscreen_window_keeps_its_place)
{
    build();
    win->fullscreen(true);

    win->configure_request(configure_request(
            client.window, CWX | CWY | CWWidth | CWHeight, 30, 40, 320, 210));

    EXPECT_EQ(frame().width, screen().width);
    EXPECT_EQ(frame().height, screen().height);
}

TEST_F(ClientTest, the_client_is_told_its_place_after_maximize_and_restore)
{
    build();
    client.watch_structure();
    XConfigureEvent notify = {};

    win->maximize();
    ASSERT_TRUE(client.synthetic_configure(notify));
    EXPECT_EQ(notify.y, tiny::wm::shell_theme.panel + header_height() +
                                frame().border_width);

    win->restore();
    ASSERT_TRUE(client.synthetic_configure(notify));
    EXPECT_EQ(notify.x, frame().x + frame().border_width);
}

TEST_F(ClientTest, the_client_is_told_its_place_after_moving_and_resizing)
{
    build();
    client.watch_structure();
    XConfigureEvent notify = {};

    win->on_window_drag_end(nullptr, make_event(ButtonRelease), nullptr);
    EXPECT_TRUE(client.synthetic_configure(notify));

    win->on_move_resize_end(nullptr, make_event(ButtonRelease), nullptr);
    EXPECT_TRUE(client.synthetic_configure(notify));
}

} // namespace

TEST_F(ClientTest, a_window_with_a_title_bar_can_be_rolled_up)
{
    build();

    const auto actions = win->get_allowed_actions();

    EXPECT_EQ(std::count(actions.begin(), actions.end(),
                         dsp()._NET_WM_ACTION_SHADE),
              1);
}

TEST_F(ClientTest, a_window_without_a_title_bar_cannot_be_rolled_up)
{
    build(0xFFFF, tiny::wm::MotifWMHints::DECOR_BORDER);

    const auto actions = win->get_allowed_actions();

    EXPECT_EQ(std::count(actions.begin(), actions.end(),
                         dsp()._NET_WM_ACTION_SHADE),
              0);
}

TEST_F(ClientTest, the_title_says_when_the_window_does_not_respond)
{
    client.set_title("Editor");
    client.set_protocols({dsp()._NET_WM_PING});
    build();
    const auto now = Probe::Clock::now();
    win->ping(now);

    win->expire_ping(now + Probe::ping_timeout);

    EXPECT_EQ(win->header.get_title(), "Editor (not responding)");
    win->pong();
    EXPECT_EQ(win->header.get_title(), "Editor");
}

TEST_F(ClientTest, the_title_that_changed_meanwhile_comes_back_when_it_responds)
{
    client.set_title("Editor");
    client.set_protocols({dsp()._NET_WM_PING});
    build();
    const auto now = Probe::Clock::now();
    win->ping(now);
    win->expire_ping(now + Probe::ping_timeout);
    client.set_title("Report");
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp().WM_NAME;
    send(client.window, e);
    EXPECT_EQ(win->header.get_title(), "Report (not responding)");

    win->pong();

    EXPECT_EQ(win->header.get_title(), "Report");
}
