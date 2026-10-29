#include <X11/cursorfont.h>

#include <gtest/gtest.h>

#include "containers.h"
#include "display.h"

class BoxTest: public ::testing::Test {
  protected:
    void realize(tiny::Box &box)
    { box.realize(XDefaultRootWindow(tiny::get_display()), 0, 0); }
};

TEST_F(BoxTest, initial_offsets_horizontal)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);

    EXPECT_EQ(box.get_start_offset(), 0u);
    EXPECT_EQ(box.get_end_offset(), 199u);
}

TEST_F(BoxTest, initial_offsets_vertical)
{
    tiny::Box box(tiny::Box::Type::Vertical, 200, 50);

    EXPECT_EQ(box.get_start_offset(), 0u);
    EXPECT_EQ(box.get_end_offset(), 49u);
}

TEST_F(BoxTest, push_start_horizontal_moves_start_offset)
{
    tiny::Box box(tiny::Box::Type::Horizontal, 200, 50);
    realize(box);
    tiny::Widget a(20, 10), b(30, 10);

    box.push_start(&a, 5);
    EXPECT_EQ(box.get_start_offset(), 25u);

    box.push_start(&b, 5);
    EXPECT_EQ(box.get_start_offset(), 60u);
}

TEST_F(BoxTest, push_start_vertical_moves_start_offset)
{
    tiny::Box box(tiny::Box::Type::Vertical, 200, 100);
    realize(box);
    tiny::Widget a(20, 10), b(30, 15);

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
    tiny::Widget a(20, 10), b(30, 10);
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
    EXPECT_EQ(tiny::Position::get_cursor_shape(tiny::Position::Top), (uint32_t)XC_top_side);
    EXPECT_EQ(tiny::Position::get_cursor_shape(0), (uint32_t)XC_arrow);
}
