#include <chrono>
#include <string>
#include <thread>

#include <X11/cursorfont.h>

#include <gtest/gtest.h>

#include "containers.h"
#include "display.h"
#include "object.h"

namespace tiny {
extern Handlers handlers;
}

class BoxTest : public ::testing::Test {
  protected:
    void realize(tiny::Box& box)
    {
        box.realize(XDefaultRootWindow(tiny::get_display()), 0, 0);
    }
};

TEST_F(BoxTest, initial_offsets_horizontal)
{
    const tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);

    EXPECT_EQ(box.get_start_offset(), 0u);
    EXPECT_EQ(box.get_end_offset(), 199u);
}

TEST_F(BoxTest, initial_offsets_vertical)
{
    const tiny::Box box(tiny::Box::Type::Vertical, 200, 50);

    EXPECT_EQ(box.get_start_offset(), 0u);
    EXPECT_EQ(box.get_end_offset(), 49u);
}

TEST_F(BoxTest, push_start_horizontal_moves_start_offset)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget b(30, 10);

    box.push_start(&a, 5);
    EXPECT_EQ(box.get_start_offset(), 25u);

    box.push_start(&b, 5);
    EXPECT_EQ(box.get_start_offset(), 60u);
}

TEST_F(BoxTest, push_start_vertical_moves_start_offset)
{
    tiny::Box box(tiny::Box::Type::Vertical, 200, 100);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget b(30, 15);

    box.push_start(&a, 0, 2);
    EXPECT_EQ(box.get_start_offset(), 12u);

    box.push_start(&b, 0, 3);
    EXPECT_EQ(box.get_start_offset(), 30u);
}

TEST_F(BoxTest, push_back_horizontal_moves_end_offset)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);

    box.push_back(&a, 5);
    EXPECT_EQ(box.get_end_offset(), 199u - 25u);
}

TEST_F(BoxTest, push_without_realize_does_not_move_widget)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    tiny::Widget a(20, 10);

    box.push_start(&a);
    EXPECT_FALSE(a.get_realized());
}

TEST_F(BoxTest, remove_start_widget_restores_start_offset)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget b(30, 10);
    box.push_start(&a, 5);
    box.push_start(&b, 5);

    box.remove(&a);

    EXPECT_EQ(box.get_start_offset(), 35u);
}

TEST_F(BoxTest, remove_end_widget_restores_end_offset)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    box.push_back(&a, 5);

    box.remove(&a);

    EXPECT_EQ(box.get_end_offset(), 199u);
}

TEST_F(BoxTest, resize_horizontal_shifts_end_offset)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);

    box.resize(250, 50);

    EXPECT_EQ(box.get_end_offset(), 249u);
}

TEST(PositionTest, cursor_shape_for_edges)
{
    EXPECT_EQ(tiny::Position::get_cursor_shape(tiny::Position::Top),
              static_cast<uint32_t>(XC_top_side));
    EXPECT_EQ(tiny::Position::get_cursor_shape(0),
              static_cast<uint32_t>(XC_arrow));
}

TEST(PositionTest, cursor_shape_for_all_sides_and_corners)
{
    using tiny::Position;
    const struct {
        uint16_t mask;
        uint32_t shape;
    } cases[] = {
            {Position::Top, XC_top_side},
            {Position::Right, XC_right_side},
            {Position::Bottom, XC_bottom_side},
            {Position::Left, XC_left_side},
            {Position::Top | Position::Right, XC_top_right_corner},
            {Position::Bottom | Position::Right, XC_bottom_right_corner},
            {Position::Top | Position::Left, XC_top_left_corner},
            {Position::Bottom | Position::Left, XC_bottom_left_corner},
            {Position::Left | Position::Right, XC_arrow}, // not implemented yet
            {Position::Top | Position::Bottom, XC_arrow}, // not implemented yet
    };

    for (auto c : cases) {
        EXPECT_EQ(Position::get_cursor_shape(c.mask), c.shape)
                << "mask " << c.mask;
    }
}

// ---- Container ----

static XWindowAttributes attributes_of(tiny::Widget& w)
{
    XSync(tiny::get_display(), False);
    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), w.get_window(), &attrs);
    return attrs;
}

class ContainerTest : public ::testing::Test {
  protected:
    ::Window root() const { return XDefaultRootWindow(tiny::get_display()); }
};

TEST_F(ContainerTest, add_realizes_child_inside_container)
{
    tiny::Container c(200, 100);
    c.realize(root(), 0, 0);
    tiny::Widget child(20, 10);

    c.add(&child, 7, 9);

    EXPECT_TRUE(child.get_realized());
    EXPECT_EQ(child.get_parent_window(), c.get_window());
    const XWindowAttributes attrs = attributes_of(child);
    EXPECT_EQ(attrs.x, 7);
    EXPECT_EQ(attrs.y, 9);
}

TEST_F(ContainerTest, add_sets_gravity)
{
    tiny::Container c(200, 100);
    c.realize(root(), 0, 0);
    tiny::Widget child(20, 10);

    c.add(&child, 0, 0, SouthEastGravity);

    EXPECT_EQ(attributes_of(child).win_gravity, SouthEastGravity);
}

TEST_F(ContainerTest, add_to_unrealized_container_reports_error)
{
    tiny::Container c(200, 100);
    tiny::Widget child(20, 10);

    ::testing::internal::CaptureStderr();
    c.add(&child);
    const std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("must be realized"), std::string::npos);
    EXPECT_FALSE(child.get_realized());
}

TEST_F(ContainerTest, map_all_maps_container_and_children)
{
    tiny::Container c(200, 100);
    c.realize(root(), 0, 0);
    tiny::Widget a(20, 10);
    tiny::Widget b(20, 10);
    c.add(&a);
    c.add(&b);

    c.map_all();

    EXPECT_TRUE(c.get_maped());
    EXPECT_TRUE(a.get_maped());
    EXPECT_TRUE(b.get_maped());
}

TEST_F(ContainerTest, removed_child_is_not_mapped_by_map_all)
{
    tiny::Container c(200, 100);
    c.realize(root(), 0, 0);
    tiny::Widget a(20, 10);
    tiny::Widget b(20, 10);
    c.add(&a);
    c.add(&b);

    c.remove(&a);
    c.map_all();

    EXPECT_FALSE(a.get_maped());
    EXPECT_TRUE(b.get_maped());
}

TEST_F(ContainerTest, delete_through_base_pointer)
{
    tiny::Widget* c = new tiny::Container(200, 100);
    c->realize(root(), 0, 0);

    delete c;
}

// ---- Box positions ----

TEST_F(BoxTest, push_start_places_widgets_one_after_another)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget b(30, 10);

    box.push_start(&a, 5, 1);
    box.push_start(&b, 5, 1);

    EXPECT_EQ(attributes_of(a).x, 5);
    EXPECT_EQ(attributes_of(a).y, 1);
    EXPECT_EQ(attributes_of(b).x, 30);
}

TEST_F(BoxTest, push_back_places_widgets_from_the_end)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget b(30, 10);

    box.push_back(&a, 5);
    box.push_back(&b, 5);

    EXPECT_EQ(attributes_of(a).x, 174);
    EXPECT_EQ(attributes_of(b).x, 139);
}

TEST_F(BoxTest, push_back_vertical_moves_end_offset_and_places_widget)
{
    tiny::Box box(tiny::Box::Type::Vertical, 100, 100);
    realize(box);
    tiny::Widget a(20, 10);

    box.push_back(&a, 3, 2);

    EXPECT_EQ(box.get_end_offset(), 99u - 12u);
    EXPECT_EQ(attributes_of(a).x, 3);
    EXPECT_EQ(attributes_of(a).y, 87);
}

TEST_F(BoxTest, resize_vertical_shifts_end_offset)
{
    tiny::Box box(tiny::Box::Type::Vertical, 100, 100);
    realize(box);

    box.resize(100, 150);

    EXPECT_EQ(box.get_end_offset(), 149u);
}

TEST_F(BoxTest, remove_start_widget_moves_following_widgets_back)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget b(30, 10);
    box.push_start(&a, 5);
    box.push_start(&b, 5);

    box.remove(&a);

    EXPECT_EQ(box.get_start_offset(), 35u);
    EXPECT_EQ(attributes_of(b).x, 5);
}

TEST_F(BoxTest, remove_end_widget_moves_remaining_end_widgets)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget b(30, 10);
    box.push_back(&a, 5);
    box.push_back(&b, 5);

    box.remove(&a);

    EXPECT_EQ(box.get_end_offset(), 164u);
    EXPECT_EQ(attributes_of(b).x, 164);
}

TEST_F(BoxTest, remove_unknown_widget_changes_nothing)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10);
    tiny::Widget stranger(20, 10);
    box.push_start(&a, 5);

    box.remove(&stranger);

    EXPECT_EQ(box.get_start_offset(), 25u);
    EXPECT_EQ(attributes_of(a).x, 5);
}

TEST_F(BoxTest, remove_from_vertical_box_is_not_implemented)
{
    tiny::Box box(tiny::Box::Type::Vertical, 100, 100);
    realize(box);
    tiny::Widget a(20, 10);
    box.push_start(&a);

    ::testing::internal::CaptureStderr();
    box.remove(&a);
    const std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("Not implemented yet"), std::string::npos);
}

TEST_F(BoxTest, delete_through_base_pointer)
{
    const tiny::Container* box =
            new tiny::Box(tiny::Box::Type::Vertical, 10, 10);

    delete box;
}

// ---- Popover ----

class PopoverTest : public ::testing::Test {
  protected:
    ::Window root() const { return XDefaultRootWindow(tiny::get_display()); }

    void focus_out(tiny::Popover& p)
    {
        XEvent e = {};
        e.type = FocusOut;
        tiny::handlers.call_hanlder({FocusOut, p.get_window()}, e);
    }
};

TEST_F(PopoverTest, default_popover_is_vertical)
{
    const tiny::Popover p(100, 50);

    EXPECT_EQ(p.get_type(), tiny::Box::Type::Vertical);
}

TEST_F(PopoverTest, realize_sets_override_redirect)
{
    tiny::Popover p(tiny::Box::Type::Horizontal, 100, 50);

    p.realize(root(), 0, 0);

    EXPECT_TRUE(attributes_of(p).override_redirect);
}

TEST_F(PopoverTest, popup_centers_under_x_and_maps)
{
    tiny::Popover p(200, 100);
    p.realize(root(), 0, 0);

    p.popup(300, 50);

    EXPECT_TRUE(p.get_maped());
    const XWindowAttributes attrs = attributes_of(p);
    EXPECT_EQ(attrs.x, 200);
    EXPECT_EQ(attrs.y, 50);
    EXPECT_EQ(attrs.map_state, IsViewable);

    ::Window focus;
    int revert;
    XGetInputFocus(tiny::get_display(), &focus, &revert);
    EXPECT_EQ(focus, p.get_window());
}

TEST_F(PopoverTest, popup_from_button_event_uses_root_coordinates)
{
    tiny::Popover p(200, 100);
    p.realize(root(), 0, 0);
    XButtonEvent be = {};
    be.x_root = 400;
    be.y_root = 70;
    be.x = 3;
    be.y = 4;

    p.popup(be);

    const XWindowAttributes attrs = attributes_of(p);
    EXPECT_EQ(attrs.x, 300);
    EXPECT_EQ(attrs.y, 70);
}

TEST_F(PopoverTest, focus_out_unmaps_popover)
{
    tiny::Popover p(200, 100);
    p.realize(root(), 0, 0);
    p.popup(300, 50);

    focus_out(p);

    EXPECT_FALSE(p.get_maped());
}

TEST_F(PopoverTest, popup_right_after_focus_out_is_ignored)
{
    tiny::Popover p(200, 100);
    p.realize(root(), 0, 0);
    p.popup(300, 50);
    focus_out(p);

    p.popup(300, 50); // click on the button that just closed the popover

    EXPECT_FALSE(p.get_maped());
}

TEST_F(PopoverTest, popup_works_again_after_timeout)
{
    tiny::Popover p(200, 100);
    p.realize(root(), 0, 0);
    p.popup(300, 50);
    focus_out(p);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    p.popup(300, 50);

    EXPECT_TRUE(p.get_maped());
}

TEST_F(PopoverTest, destructor_disconnects_focus_handler)
{
    ::Window window;
    {
        tiny::Popover p(200, 100);
        p.realize(root(), 0, 0);
        window = p.get_window();
        EXPECT_EQ(tiny::handlers.count({FocusOut, window}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({FocusOut, window}), 0u);
}

TEST_F(PopoverTest, delete_through_base_pointer)
{
    tiny::Box* p = new tiny::Popover(200, 100);
    p->realize(root(), 0, 0);

    delete p;
}
