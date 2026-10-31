#include "fake_client.h"
#include "helpers.h"

#include <memory>

#include "wm/client.h"

namespace {

// Client with the members the tests need to look at.
class Probe : public tiny::wm::Client {
  public:
    using tiny::wm::Client::Client;
    using tiny::wm::Client::cls_btn;
    using tiny::wm::Client::header;
    using tiny::wm::Client::max_btn;
    using tiny::wm::Client::min_btn;
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
    explicit FrameMinWidth(int width): old(tiny::theme.wm_win_min_width)
    {
        tiny::theme.wm_win_min_width = width;
    }

    ~FrameMinWidth() { tiny::theme.wm_win_min_width = old; }

  private:
    int old;
};

class ClientTest : public ::testing::Test {
  protected:
    void SetUp() override { client.map(); }

    // Properties of the client must be set before.
    Probe* build(unsigned long functions = 0xFFFF)
    {
        const XWindowAttributes a = client.attrs();
        win = std::make_unique<Probe>(client.window, root_window(), a.width,
                                      a.height, functions, 0xFFFF);
        win->setup(root_window(), a.x, a.y);
        pump_events();
        return win.get();
    }

    XWindowAttributes frame() { return attributes_of(win->get_window()); }

    XWindowAttributes child() { return client.attrs(); }

    XWindowAttributes screen() { return attributes_of(root_window()); }

    int header_height() const { return tiny::theme.wm_win_header; }

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

    EXPECT_EQ(children_of(win->header.get_window()).size(), 4u); // + title
}

TEST_F(ClientTest, window_without_maximize_function_has_no_maximize_button)
{
    build(tiny::wm::MotifWMHints::FUNC_RESIZE);

    EXPECT_EQ(children_of(win->header.get_window()).size(), 3u);
}

TEST_F(ClientTest, create_sets_wm_state_to_normal)
{
    const XWindowAttributes a = client.attrs();
    const std::unique_ptr<tiny::wm::Window> w(tiny::wm::Client::create(
            root_window(), client.window, a, 0xFFFF, 0xFFFF));
    const ::Window frame =
            static_cast<tiny::wm::Client*>(w.get())->get_window();
    XSync(dsp(), False);

    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dsp(), frame, dsp().WM_STATE, 0, 2, False,
                                 dsp().WM_STATE, &type, &format, &count, &left,
                                 &data),
              Success);
    ASSERT_EQ(count, 2u);
    EXPECT_EQ((reinterpret_cast<long*>(data))[0], NormalState);
    XFree(data);
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

TEST_F(ClientTest, maximize_button_fills_the_screen)
{
    build();

    click(win->max_btn);

    auto s = screen();
    auto f = frame();
    EXPECT_TRUE(win->get_maximized());
    EXPECT_EQ(f.x, 0);
    EXPECT_EQ(f.y, 0);
    EXPECT_EQ(f.width, s.width);
    EXPECT_EQ(f.height, s.height);
    EXPECT_EQ(child().width, s.width);
    EXPECT_EQ(child().height, s.height - header_height());
}

TEST_F(ClientTest, maximize_button_restores_a_maximized_window)
{
    build();
    click(win->max_btn);

    click(win->max_btn);

    auto f = frame();
    EXPECT_FALSE(win->get_maximized());
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, 20);
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
    Probe second(other.window, root_window(), a.width, a.height, 0xFFFF,
                 0xFFFF);
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
    ASSERT_NE(tiny::theme.wm_header.get_fg(
                      static_cast<uint8_t>(tiny::Style::State::Normal)),
              tiny::theme.wm_header.get_fg(
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

TEST_F(ClientTest, focus_in_signals_on_focus)
{
    build();
    Recorder r;
    win->on_focus.connect(&r, r.slot());

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

TEST_F(ClientTest, normal_hints_change_puts_a_moved_client_back)
{
    build();
    XMoveWindow(client.display, client.window, 30, 40);
    client.sync();
    ASSERT_EQ(child().x, 30);
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp().WM_NORMAL_HINTS;

    send(client.window, e);

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
    EXPECT_GE(frame().width, tiny::theme.wm_win_min_width);
    EXPECT_GE(frame().height, tiny::theme.wm_win_min_height);
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

} // namespace
