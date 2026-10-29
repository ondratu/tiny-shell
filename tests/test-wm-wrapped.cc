#include "client.h"
#include "helpers.h"

#include <memory>

#include "wm_wrapped.h"

namespace {

// Wrapped with the members the tests need to look at.
class Probe: public wm::Wrapped {
  public:
    using wm::Wrapped::Wrapped;
    using wm::Wrapped::header;
    using wm::Wrapped::cls_btn;
    using wm::Wrapped::min_btn;
    using wm::Wrapped::max_btn;
    using wm::Wrapped::shadow;

    // What Wrapped::create does, without the WM_STATE property.
    void setup(::Window root_arg, int x, int y)
    {
        realize(root_arg, x, y);
        on_focus_out(XEvent(), nullptr);
        map_all();
    }
};

class WrappedTest: public ::testing::Test {
  protected:
    void SetUp() override
    { client.map(); }

    // Properties of the client must be set before.
    Probe *build(unsigned long functions = 0xFFFF)
    {
        XWindowAttributes a = client.attrs();
        win = std::make_unique<Probe>(client.window, root_window(),
                a.width, a.height, functions, 0xFFFF);
        win->setup(root_window(), a.x, a.y);
        pump_events();
        return win.get();
    }

    XWindowAttributes frame()
    { return attributes_of(win->get_window()); }

    XWindowAttributes child()
    { return client.attrs(); }

    XWindowAttributes screen()
    { return attributes_of(root_window()); }

    uint32_t header_height() const
    { return tiny::theme.wm_win_header; }

    // Click on a button of the header.
    void click(tiny::Button &button)
    { button.on_click(&button, make_event(ButtonRelease)); }

    // Limits resizing: 50x50 .. 800x600
    void resizable()
    { client.set_size_hints(PMinSize|PMaxSize, 50, 50, 800, 600); }

    XEvent pointer(int x_root, int y_root, int type = ButtonPress)
    {
        XEvent e = make_event(type);
        e.xbutton.x_root = x_root;
        e.xbutton.y_root = y_root;
        return e;
    }

    HeaderFont header_font;
    WidgetFont widget_font;
    Client client;     // 10,20 200x100
    std::unique_ptr<Probe> win;
};

// ---- create ----

TEST_F(WrappedTest, frame_wraps_the_client)
{
    build();

    auto f = frame();
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, 20);
    EXPECT_EQ(f.width, 200);
    EXPECT_EQ(f.height, (int) (100 + header_height()));
    EXPECT_EQ(f.map_state, IsViewable);
}

TEST_F(WrappedTest, client_is_reparented_under_the_header)
{
    build();

    auto c = child();
    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, (int) header_height());
    EXPECT_EQ(c.border_width, 0);
    EXPECT_EQ(c.width, 200);
    EXPECT_EQ(c.height, 100);

    auto kids = children_of(win->get_window());
    EXPECT_NE(std::find(kids.begin(), kids.end(), client.window), kids.end());
}

TEST_F(WrappedTest, title_comes_from_the_client)
{
    client.set_title("hello");
    build();

    EXPECT_EQ(win->header.get_title(), "hello");
}

TEST_F(WrappedTest, header_has_three_buttons)
{
    build();

    EXPECT_EQ(children_of(win->header.get_window()).size(), 4u);   // + title
}

TEST_F(WrappedTest, window_without_maximize_function_has_no_maximize_button)
{
    build(wm::MotifWMHints::FUNC_RESIZE);

    EXPECT_EQ(children_of(win->header.get_window()).size(), 3u);
}

TEST_F(WrappedTest, create_sets_wm_state_to_normal)
{
    XWindowAttributes a = client.attrs();
    std::unique_ptr<wm::Window> w(wm::Wrapped::create(root_window(),
                client.window, a, 0xFFFF, 0xFFFF));
    ::Window frame = static_cast<wm::Wrapped *>(w.get())->get_window();
    XSync(dsp(), False);

    Atom type;
    int format;
    unsigned long count, left;
    unsigned char *data = nullptr;
    ASSERT_EQ(XGetWindowProperty(dsp(), frame, dsp().WM_STATE, 0, 2, False,
                dsp().WM_STATE, &type, &format, &count, &left, &data),
            Success);
    ASSERT_EQ(count, 2u);
    EXPECT_EQ(((long *) data)[0], NormalState);
    XFree(data);
}

TEST_F(WrappedTest, resizable_window_has_a_shadow_below_the_frame)
{
    build();

    auto stack = children_of(root_window());
    ASSERT_GE(stack.size(), 2u);
    EXPECT_EQ(stack.back(), win->get_window());
    EXPECT_EQ(stack[stack.size() - 2], win->shadow.get_window());
}

TEST_F(WrappedTest, window_without_resize_function_has_no_shadow)
{
    build(wm::MotifWMHints::FUNC_MOVE);

    EXPECT_FALSE(win->is_resizable());
    auto stack = children_of(root_window());
    EXPECT_EQ(stack.back(), win->get_window());
    EXPECT_EQ(std::find(stack.begin(), stack.end(),
                win->shadow.get_window()), stack.end());
}

// ---- minimize, maximize ----

TEST_F(WrappedTest, minimize_button_hides_the_window)
{
    build();

    click(win->min_btn);

    EXPECT_TRUE(win->get_minimized());
    EXPECT_EQ(frame().map_state, IsUnmapped);
}

TEST_F(WrappedTest, restore_shows_a_minimized_window)
{
    build();
    click(win->min_btn);

    win->restore();

    EXPECT_FALSE(win->get_minimized());
    EXPECT_EQ(frame().map_state, IsViewable);
}

TEST_F(WrappedTest, maximize_button_fills_the_screen)
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
    EXPECT_EQ(child().height, s.height - (int) header_height());
}

TEST_F(WrappedTest, maximize_button_restores_a_maximized_window)
{
    build();
    click(win->max_btn);

    click(win->max_btn);

    auto f = frame();
    EXPECT_FALSE(win->get_maximized());
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, 20);
    EXPECT_EQ(f.width, 200);
    EXPECT_EQ(f.height, (int) (100 + header_height()));
    EXPECT_EQ(child().width, 200);
    EXPECT_EQ(child().height, 100);
}

TEST_F(WrappedTest, maximize_twice_keeps_the_first_restore_state)
{
    build();
    win->maximize();
    pump_events();

    win->maximize();
    win->restore();

    EXPECT_EQ(frame().x, 10);
    EXPECT_EQ(frame().width, 200);
}

TEST_F(WrappedTest, restore_can_center_the_window_on_the_pointer)
{
    build();
    win->maximize();

    win->restore(500, 400);

    EXPECT_EQ(frame().x, 500 - 100);
    EXPECT_EQ(frame().y, 400 - (int) (100 + header_height()) / 2);
}

// ---- close ----

TEST_F(WrappedTest, close_button_asks_the_client_to_exit)
{
    client.set_protocols({dsp().WM_DELETE_WINDOW});
    build();

    click(win->cls_btn);

    XEvent e;
    ASSERT_TRUE(client.message(e));
    EXPECT_EQ(e.xclient.data.l[0], (long) dsp().WM_DELETE_WINDOW);
}

TEST_F(WrappedTest, close_button_kills_an_uncooperative_client)
{
    build();

    click(win->cls_btn);
    XSync(dsp(), False);

    XErrors errors;
    XWindowAttributes attrs;
    XGetWindowAttributes(dsp(), client.window, &attrs);
    EXPECT_EQ(errors.errors(), 1);
    client.abandon();
}

// ---- focus ----

TEST_F(WrappedTest, set_focus_raises_the_window)
{
    build();
    Client other(100, 100);
    other.map();
    XWindowAttributes a = other.attrs();
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

TEST_F(WrappedTest, focus_events_enable_and_disable_the_header)
{
    client.set_title("title");
    build();
    // only the text color differs
    ASSERT_NE(tiny::theme.wm_header.get_fg((uint8_t) tiny::Style::State::Normal),
            tiny::theme.wm_header.get_fg(
                (uint8_t) tiny::Style::State::Disabled));
    auto header_image = [this] {
        send(win->header, Expose);
        return Image(win->header.get_window(), win->header.get_width(),
                win->header.get_height());
    };
    Image disabled = header_image();

    send(client.window, make_event(FocusIn));
    Image enabled = header_image();
    EXPECT_FALSE(enabled == disabled);

    send(client.window, make_event(FocusOut));
    EXPECT_TRUE(header_image() == disabled);
}

TEST_F(WrappedTest, focus_in_signals_on_focus)
{
    build();
    Recorder r;
    win->on_focus.connect(&r, r.slot());

    send(client.window, make_event(FocusIn));

    EXPECT_EQ(r.calls, 1);
}

// ---- properties ----

TEST_F(WrappedTest, title_follows_wm_name)
{
    build();
    client.set_title("changed");
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp().WM_NAME;

    send(client.window, e);

    EXPECT_EQ(win->header.get_title(), "changed");
}

TEST_F(WrappedTest, title_follows_net_wm_name)
{
    build();
    client.set_utf8(dsp()._NET_WM_NAME, "utf8 title");
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp()._NET_WM_NAME;

    send(client.window, e);

    EXPECT_EQ(win->header.get_title(), "utf8 title");
}

TEST_F(WrappedTest, normal_hints_change_puts_a_moved_client_back)
{
    build();
    XMoveWindow(client.display, client.window, 30, 40);
    client.sync();
    ASSERT_EQ(child().x, 30);
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp().WM_NORMAL_HINTS;

    send(client.window, e);

    EXPECT_EQ(child().x, 0);
    EXPECT_EQ(child().y, (int) header_height());
}

TEST_F(WrappedTest, normal_hints_change_of_a_gone_client_is_survived)
{
    build();
    XErrors errors;
    XDestroyWindow(client.display, client.window);
    client.sync();
    XEvent e = make_event(PropertyNotify);
    e.xproperty.atom = dsp().WM_NORMAL_HINTS;

    send(client.window, e);
}

// ---- move ----

TEST_F(WrappedTest, dragging_the_title_moves_the_window)
{
    build();
    auto *title = win->header.get_title_box();

    title->on_drag_begin(title, pointer(100, 100));
    title->on_drag_motion(title, pointer(130, 140, MotionNotify));

    EXPECT_EQ(frame().x, 40);
    EXPECT_EQ(frame().y, 60);
    EXPECT_TRUE(win->is_focused());
}

TEST_F(WrappedTest, dragging_the_title_moves_the_shadow_too)
{
    build();
    auto *title = win->header.get_title_box();
    int before = attributes_of(win->shadow).x;

    title->on_drag_begin(title, pointer(100, 100));
    title->on_drag_motion(title, pointer(130, 140, MotionNotify));

    EXPECT_EQ(attributes_of(win->shadow).x, before + 30);
}

TEST_F(WrappedTest, dragging_a_maximized_window_restores_it)
{
    build();
    win->maximize();
    auto *title = win->header.get_title_box();
    XEvent e = pointer(500, 10);
    e.xmotion.x = 500;
    e.xmotion.y = 10;

    title->on_drag_begin(title, e);

    EXPECT_FALSE(win->get_maximized());
    EXPECT_EQ(frame().width, 200);
}

// ---- resize ----

TEST_F(WrappedTest, resizing_the_right_bottom_corner_grows_the_window)
{
    resizable();
    build();
    using tiny::Position;
    void *mask = (void *) (size_t) (Position::Right | Position::Bottom);

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(350, 330, MotionNotify), mask);

    pump_events();      // ConfigureNotify updates the widget sizes
    auto f = frame();
    EXPECT_EQ(f.x, 10);
    EXPECT_EQ(f.y, 20);
    EXPECT_EQ(f.width, 250);
    EXPECT_EQ(f.height, (int) (100 + header_height()) + 30);
    EXPECT_EQ(child().width, 250);
    EXPECT_EQ(child().height, 130);
    EXPECT_EQ(win->header.get_width(), 250u);
}

TEST_F(WrappedTest, resizing_the_left_top_corner_moves_the_window)
{
    resizable();
    build();
    using tiny::Position;
    void *mask = (void *) (size_t) (Position::Left | Position::Top);

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(280, 270, MotionNotify), mask);

    auto f = frame();
    EXPECT_EQ(f.x, -10);
    EXPECT_EQ(f.y, -10);
    EXPECT_EQ(f.width, 220);
    EXPECT_EQ(f.height, (int) (100 + header_height()) + 30);
}

TEST_F(WrappedTest, resizing_is_limited_by_the_size_hints)
{
    resizable();
    build();
    using tiny::Position;
    void *mask = (void *) (size_t) (Position::Right | Position::Bottom);

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(5000, 5000, MotionNotify),
            mask);
    EXPECT_EQ(frame().width, 800);
    EXPECT_EQ(frame().height, 600);

    win->on_move_resize_motion(nullptr, pointer(-5000, -5000, MotionNotify),
            mask);
    EXPECT_GE(frame().width, (int) tiny::theme.wm_win_min_width);
    EXPECT_GE(frame().height, (int) tiny::theme.wm_win_min_height);
}

TEST_F(WrappedTest, resizing_snaps_to_the_resize_increment)
{
    XSizeHints hints = {};
    hints.flags = PMinSize|PMaxSize|PResizeInc;
    hints.min_width = hints.min_height = 50;
    hints.max_width = hints.max_height = 800;
    hints.width_inc = 10;
    hints.height_inc = 10;
    XSetWMNormalHints(client.display, client.window, &hints);
    client.sync();
    build();
    using tiny::Position;
    void *mask = (void *) (size_t) Position::Right;

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);
    win->on_move_resize_motion(nullptr, pointer(391, 300, MotionNotify), mask);

    EXPECT_EQ(frame().width, 290);
}

TEST_F(WrappedTest, resizing_unmaximizes_the_window)
{
    resizable();
    build();
    win->maximize();

    win->on_move_resize_begin(nullptr, pointer(300, 300), nullptr);

    EXPECT_FALSE(win->get_maximized());
}

// ---- life ----

TEST_F(WrappedTest, destructor_disconnects_handlers_and_destroys_the_frame)
{
    build();
    ::Window f = win->get_window();
    ::Window kid = client.window;
    ASSERT_TRUE(tiny::handlers.count({FocusIn, kid}));

    win.reset();

    for (int type: {FocusIn, FocusOut, PropertyNotify}) {
        EXPECT_FALSE(tiny::handlers.count({type, kid})) << type;
    }
    XErrors errors;
    XWindowAttributes attrs;
    XGetWindowAttributes(dsp(), f, &attrs);
    EXPECT_EQ(errors.errors(), 1);     // BadWindow
}

TEST_F(WrappedTest, destructor_gives_the_client_back_to_the_root)
{
    build();

    win.reset();

    ::Window root, parent, *kids = nullptr;
    unsigned int count;
    ASSERT_TRUE(XQueryTree(dsp(), client.window, &root, &parent, &kids,
                &count));
    if (kids) {
        XFree(kids);
    }
    EXPECT_EQ(parent, root_window());
    auto c = child();
    EXPECT_EQ(c.x, 10);
    EXPECT_EQ(c.y, 20 + (int) header_height());
    EXPECT_EQ(c.width, 200);
}

TEST_F(WrappedTest, destructor_survives_a_client_that_is_gone)
{
    build();
    XDestroyWindow(client.display, client.window);
    client.sync();

    XErrors errors;
    win.reset();

    EXPECT_EQ(errors.errors(), 0);
}

} // namespace
