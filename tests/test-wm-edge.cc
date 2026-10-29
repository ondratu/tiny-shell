#include "helpers.h"

#include <X11/cursorfont.h>

#include "wm_edge.h"

// ---- Edge ----

TEST(EdgeTest, keeps_mask_and_size)
{
    const wm::Edge edge(20, 10, tiny::Position::Top);

    EXPECT_EQ(edge.get_mask(), tiny::Position::Top);
    EXPECT_EQ(edge.get_width(), 20u);
    EXPECT_EQ(edge.get_height(), 10u);
}

TEST(EdgeTest, realize_creates_input_only_window)
{
    wm::Edge edge(20, 10, tiny::Position::Top);

    edge.realize(root_window(), 3, 4);

    const XWindowAttributes attrs = attributes_of(edge);
    EXPECT_EQ(attrs.c_class, InputOnly);
    EXPECT_EQ(attrs.x, 3);
    EXPECT_EQ(attrs.y, 4);
    EXPECT_EQ(attrs.width, 20);
    EXPECT_EQ(attrs.height, 10);
}

TEST(EdgeTest, drag_signals_follow_pointer_events)
{
    wm::Edge edge(20, 10, tiny::Position::Left);
    Recorder begin;
    Recorder end;
    Recorder motion;
    edge.on_drag_begin.connect(&begin, begin.slot());
    edge.on_drag_end.connect(&end, end.slot());
    edge.on_drag_motion.connect(&motion, motion.slot());
    edge.realize(root_window(), 0, 0);

    send(edge, ButtonPress);
    EXPECT_EQ(begin.calls, 1);
    EXPECT_EQ(begin.last_owner, &edge);
    EXPECT_EQ(begin.last_event.type, ButtonPress);

    XEvent move = make_event(MotionNotify);
    move.xmotion.window = edge.get_window();
    move.xmotion.x_root = 123;
    send(edge.get_window(), move);
    EXPECT_EQ(motion.calls, 1);
    EXPECT_EQ(motion.last_event.xmotion.x_root, 123);

    send(edge, ButtonRelease);
    EXPECT_EQ(end.calls, 1);
    EXPECT_EQ(end.last_owner, &edge);
}

TEST(EdgeTest, drag_without_connected_signals_is_ignored)
{
    wm::Edge edge(20, 10, tiny::Position::Left);
    edge.realize(root_window(), 0, 0);

    send(edge, ButtonPress);
    send(edge, ButtonRelease);
}

TEST(EdgeTest, mapped_edge_can_grab_the_pointer)
{
    wm::Edge edge(20, 10, tiny::Position::Left);
    edge.realize(root_window(), 0, 0);
    edge.map();

    send(edge, ButtonPress);
    send(edge, ButtonRelease); // releases the grab
}

TEST(EdgeTest, destructor_disconnects_handlers)
{
    ::Window window;
    {
        wm::Edge edge(20, 10, tiny::Position::Left);
        edge.realize(root_window(), 0, 0);
        window = edge.get_window();
        EXPECT_EQ(tiny::handlers.count({ButtonPress, window}), 1u);
        EXPECT_EQ(tiny::handlers.count({ButtonRelease, window}), 1u);
        EXPECT_EQ(tiny::handlers.count({MotionNotify, window}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({ButtonPress, window}), 0u);
    EXPECT_EQ(tiny::handlers.count({ButtonRelease, window}), 0u);
    EXPECT_EQ(tiny::handlers.count({MotionNotify, window}), 0u);
}

TEST(EdgeTest, destroying_unrealized_edge_causes_no_x_error)
{
    const XErrors x;
    {
        const wm::Edge edge(20, 10, tiny::Position::Left);
    }

    EXPECT_EQ(x.errors(), 0);
}

TEST(EdgeTest, delete_through_base_pointer)
{
    const tiny::Widget* edge = new wm::Edge(20, 10, tiny::Position::Left);

    delete edge;
}

// ---- BackWindow ----

class BackWindowTest : public ::testing::Test {
  protected:
    int border() const { return tiny::theme.wm_win_border; }
    int corner() const { return tiny::theme.wm_win_corner; }

    // client area 200x100 at 50, 60 -> 220x120 window at 40, 50
    void SetUp() override { bw = new wm::BackWindow(200, 100); }

    void TearDown() override { delete bw; }

    void expect_geometry(::Window w, int x, int y, int width, int height)
    {
        ASSERT_NE(w, 0u);
        const XWindowAttributes attrs = attributes_of(w);
        EXPECT_EQ(attrs.x, x);
        EXPECT_EQ(attrs.y, y);
        EXPECT_EQ(attrs.width, width);
        EXPECT_EQ(attrs.height, height);
    }

    wm::BackWindow* bw;
};

TEST_F(BackWindowTest, window_is_larger_than_client_area_by_the_borders)
{
    EXPECT_EQ(bw->get_width(), 200 + 2 * border());
    EXPECT_EQ(bw->get_height(), 100 + 2 * border());
}

TEST_F(BackWindowTest, realize_places_window_around_client_area)
{
    bw->realize(root_window(), 50, 60);

    const XWindowAttributes attrs = attributes_of(*bw);
    EXPECT_EQ(attrs.x, 50 - border());
    EXPECT_EQ(attrs.y, 60 - border());
    EXPECT_EQ(attrs.width, 220);
    EXPECT_EQ(attrs.height, 120);
    EXPECT_EQ(attrs.c_class, InputOnly);
}

TEST_F(BackWindowTest, realize_adds_eight_edges_and_corners)
{
    bw->realize(root_window(), 50, 60);
    const ::Window w = bw->get_window();

    EXPECT_EQ(children_of(w).size(), 8u);
    // corners
    expect_geometry(child_at(w, 0, 0), 0, 0, corner(), corner());
    expect_geometry(child_at(w, 200, 0), 200, 0, corner(), corner());
    expect_geometry(child_at(w, 0, 100), 0, 100, corner(), corner());
    expect_geometry(child_at(w, 200, 100), 200, 100, corner(), corner());
    // edges
    expect_geometry(child_at(w, 20, 0), 20, 0, 180, border());
    expect_geometry(child_at(w, 210, 20), 210, 20, border(), 80);
    expect_geometry(child_at(w, 20, 110), 20, 110, 180, border());
    expect_geometry(child_at(w, 0, 20), 0, 20, border(), 80);
}

TEST_F(BackWindowTest, move_moves_the_window_around_the_client_area)
{
    bw->realize(root_window(), 50, 60);

    bw->move(100, 120);

    const XWindowAttributes attrs = attributes_of(*bw);
    EXPECT_EQ(attrs.x, 100 - border());
    EXPECT_EQ(attrs.y, 120 - border());
    EXPECT_EQ(attrs.width, 220);
}

TEST_F(BackWindowTest, move_resize_changes_position_and_size)
{
    bw->realize(root_window(), 50, 60);

    bw->move_resize(100, 120, 300, 200);

    const XWindowAttributes attrs = attributes_of(*bw);
    EXPECT_EQ(attrs.x, 100 - border());
    EXPECT_EQ(attrs.y, 120 - border());
    EXPECT_EQ(attrs.width, 300 + 2 * border());
    EXPECT_EQ(attrs.height, 200 + 2 * border());
}

TEST_F(BackWindowTest, move_and_move_resize_before_realize_do_nothing)
{
    bw->move(100, 120);
    bw->move_resize(100, 120, 300, 200);

    EXPECT_EQ(bw->get_width(), 220u);
    EXPECT_EQ(bw->get_height(), 120u);
    EXPECT_FALSE(bw->get_realized());
}

TEST_F(BackWindowTest, resize_adds_the_borders)
{
    bw->realize(root_window(), 50, 60);

    bw->resize(300, 200);

    const XWindowAttributes attrs = attributes_of(*bw);
    EXPECT_EQ(attrs.width, 300 + 2 * border());
    EXPECT_EQ(attrs.height, 200 + 2 * border());
}

TEST_F(BackWindowTest, configure_notify_keeps_edges_on_the_sides)
{
    bw->realize(root_window(), 50, 60);
    bw->resize(300, 200); // window 320x220
    XSync(tiny::get_display(), False);

    XEvent e = make_event(ConfigureNotify);
    e.xconfigure.window = bw->get_window();
    e.xconfigure.width = 320;
    e.xconfigure.height = 220;
    send(bw->get_window(), e);

    const ::Window w = bw->get_window();
    EXPECT_EQ(bw->get_width(), 320u);
    EXPECT_EQ(bw->get_height(), 220u);
    expect_geometry(child_at(w, 20, 0), 20, 0, 280, border());     // top
    expect_geometry(child_at(w, 310, 20), 310, 20, border(), 180); // right
    expect_geometry(child_at(w, 20, 210), 20, 210, 280, border()); // bottom
    expect_geometry(child_at(w, 0, 20), 0, 20, border(), 180);     // left
}

TEST_F(BackWindowTest, corners_follow_the_window_by_gravity)
{
    bw->realize(root_window(), 50, 60);

    bw->resize(300, 200);

    const ::Window w = bw->get_window();
    expect_geometry(child_at(w, 300, 0), 300, 0, corner(), corner());
    expect_geometry(child_at(w, 300, 200), 300, 200, corner(), corner());
    expect_geometry(child_at(w, 0, 200), 0, 200, corner(), corner());
}

TEST_F(BackWindowTest, edge_drag_is_reported_with_the_edge_mask)
{
    Recorder begin;
    Recorder motion;
    bw->on_move_resize_begin.connect(&begin, begin.slot());
    bw->on_move_resize_motion.connect(&motion, motion.slot());
    bw->realize(root_window(), 50, 60);
    const ::Window top_right = child_at(bw->get_window(), 200, 0);
    ASSERT_NE(top_right, 0u);

    send(top_right, make_event(ButtonPress));
    EXPECT_EQ(begin.calls, 1);
    EXPECT_EQ(begin.last_owner, bw);

    XEvent move = make_event(MotionNotify);
    move.xmotion.window = top_right;
    send(top_right, move);
    EXPECT_EQ(motion.calls, 1);
    EXPECT_EQ(motion.last_owner, bw);
    EXPECT_EQ(motion.last_data, reinterpret_cast<void*>(tiny::Position::Right |
                                                        tiny::Position::Top));

    send(top_right, make_event(ButtonRelease));
}

TEST_F(BackWindowTest, every_edge_reports_its_own_mask)
{
    Recorder motion;
    bw->on_move_resize_motion.connect(&motion, motion.slot());
    bw->realize(root_window(), 50, 60);
    const ::Window w = bw->get_window();

    const struct {
        int x, y;
        uint16_t mask;
    } edges[] = {
            {0, 0, tiny::Position::Left | tiny::Position::Top},
            {200, 100, tiny::Position::Right | tiny::Position::Bottom},
            {0, 100, tiny::Position::Left | tiny::Position::Bottom},
            {20, 0, tiny::Position::Top},
            {210, 20, tiny::Position::Right},
            {20, 110, tiny::Position::Bottom},
            {0, 20, tiny::Position::Left},
    };
    for (auto edge : edges) {
        const ::Window child = child_at(w, edge.x, edge.y);
        ASSERT_NE(child, 0u);
        XEvent move = make_event(MotionNotify);
        move.xmotion.window = child;

        send(child, move);

        EXPECT_EQ(motion.last_data, reinterpret_cast<void*>(edge.mask))
                << edge.x << "," << edge.y;
    }
    EXPECT_EQ(motion.calls, 7);
}

TEST_F(BackWindowTest, destructor_destroys_edges_and_disconnects_handlers)
{
    bw->realize(root_window(), 50, 60);
    const ::Window w = bw->get_window();
    const ::Window child = child_at(w, 0, 0);
    ASSERT_EQ(tiny::handlers.count({ButtonPress, child}), 1u);

    delete bw;
    bw = new wm::BackWindow(10, 10); // for TearDown

    EXPECT_EQ(tiny::handlers.count({ButtonPress, child}), 0u);
    EXPECT_EQ(tiny::handlers.count({ConfigureNotify, w}), 0u);
}

TEST_F(BackWindowTest, destroying_unrealized_back_window_causes_no_x_error)
{
    const XErrors x;
    delete bw;
    bw = new wm::BackWindow(10, 10);

    EXPECT_EQ(x.errors(), 0);
}
