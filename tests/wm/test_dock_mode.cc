#include <gtest/gtest.h>

#include "wm/dock_mode.h"

using tiny::wm::dock_shown;
using tiny::wm::DockMode;
using tiny::wm::overlaps;
using tiny::wm::parse_dock_mode;

TEST(DockModeTest, the_names_of_the_modes)
{
    EXPECT_EQ(parse_dock_mode("autohide"), DockMode::AutoHide);
    EXPECT_EQ(parse_dock_mode("auto"), DockMode::AutoHide);
    EXPECT_EQ(parse_dock_mode("on"), DockMode::On);
    EXPECT_EQ(parse_dock_mode("always"), DockMode::On);
    EXPECT_EQ(parse_dock_mode("off"), DockMode::Off);
}

TEST(DockModeTest, the_case_and_the_blanks_are_ignored)
{
    EXPECT_EQ(parse_dock_mode(" AutoHide "), DockMode::AutoHide);
    EXPECT_EQ(parse_dock_mode("OFF"), DockMode::Off);
}

TEST(DockModeTest, another_text_is_not_a_mode)
{
    EXPECT_FALSE(parse_dock_mode(""));
    EXPECT_FALSE(parse_dock_mode("sometimes"));
}

TEST(DockModeTest, the_modes_have_the_names_the_file_uses)
{
    for (const DockMode mode :
         {DockMode::AutoHide, DockMode::On, DockMode::Off}) {
        EXPECT_EQ(parse_dock_mode(dock_mode_name(mode)), mode);
    }
}

TEST(DockModeTest, rectangles_overlap_when_they_share_a_pixel)
{
    EXPECT_TRUE(overlaps({0, 0, 10, 10}, {9, 9, 10, 10}));
    EXPECT_FALSE(overlaps({0, 0, 10, 10}, {10, 0, 10, 10}));
    EXPECT_FALSE(overlaps({0, 0, 10, 10}, {0, 10, 10, 10}));
    EXPECT_TRUE(overlaps({0, 0, 100, 100}, {40, 40, 5, 5}));
}

TEST(DockModeTest, an_empty_rectangle_overlaps_nothing)
{
    EXPECT_FALSE(overlaps({5, 5, 0, 10}, {0, 0, 100, 100}));
    EXPECT_FALSE(overlaps({0, 0, 100, 100}, {5, 5, 10, 0}));
}

TEST(DockModeTest, autohide_hides_the_dock_only_under_a_window)
{
    EXPECT_TRUE(dock_shown(DockMode::AutoHide, false, false));
    EXPECT_FALSE(dock_shown(DockMode::AutoHide, true, false));
}

TEST(DockModeTest, the_pointer_at_the_edge_shows_a_covered_dock)
{
    EXPECT_TRUE(dock_shown(DockMode::AutoHide, true, true));
}

TEST(DockModeTest, on_and_off_ignore_the_window_and_the_pointer)
{
    EXPECT_TRUE(dock_shown(DockMode::On, true, false));
    EXPECT_FALSE(dock_shown(DockMode::Off, false, true));
    EXPECT_FALSE(dock_shown(DockMode::Off, false, false));
}
