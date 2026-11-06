#include <gtest/gtest.h>

#include "wm/switcher_layout.h"

using tiny::wm::cells_that_fit;
using tiny::wm::visible_cells;

TEST(SwitcherLayoutTest, all_the_cells_are_shown_when_they_fit)
{
    const auto range = visible_cells(5, 4, 5);

    EXPECT_EQ(range.first, 0u);
    EXPECT_EQ(range.count, 5u);
}

TEST(SwitcherLayoutTest, no_cells_are_no_range)
{
    EXPECT_EQ(visible_cells(0, 0, 3).count, 0u);
}

TEST(SwitcherLayoutTest, the_selected_cell_is_in_the_middle_of_the_shown_ones)
{
    const auto range = visible_cells(10, 5, 5);

    EXPECT_EQ(range.first, 3u);
    EXPECT_EQ(range.count, 5u);
}

TEST(SwitcherLayoutTest, the_row_stays_between_its_ends)
{
    EXPECT_EQ(visible_cells(10, 0, 5).first, 0u);
    EXPECT_EQ(visible_cells(10, 1, 5).first, 0u);
    EXPECT_EQ(visible_cells(10, 9, 5).first, 5u);
    EXPECT_EQ(visible_cells(10, 8, 5).first, 5u);
}

TEST(SwitcherLayoutTest, a_selection_behind_the_end_is_the_last_cell)
{
    const auto range = visible_cells(10, 99, 4);

    EXPECT_EQ(range.first, 6u);
}

TEST(SwitcherLayoutTest, at_least_one_cell_is_shown)
{
    const auto range = visible_cells(10, 7, 0);

    EXPECT_EQ(range.count, 1u);
    EXPECT_EQ(range.first, 7u);
}

TEST(SwitcherLayoutTest, the_number_of_cells_that_fit_is_at_least_one)
{
    EXPECT_EQ(cells_that_fit(1000, 96), 10u);
    EXPECT_EQ(cells_that_fit(50, 96), 1u);
    EXPECT_EQ(cells_that_fit(-5, 96), 1u);
    EXPECT_EQ(cells_that_fit(500, 0), 1u);
}
