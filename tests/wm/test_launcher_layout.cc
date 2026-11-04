#include <gtest/gtest.h>

#include "theme.h"
#include "wm/shell_theme.h"
#include "wm/launcher_layout.h"

namespace {

// The default theme, not scaled; a local one, so the global is not touched
struct Themes {
    tiny::Theme toolkit;
    tiny::wm::ShellTheme shell{toolkit};
};

tiny::wm::LauncherLayout layout_of(int width, int height)
{
    const Themes themes;
    return tiny::wm::LauncherLayout::compute(width, height, themes.shell);
}

} // namespace

TEST(LauncherLayoutTest, a_full_hd_screen_keeps_the_default_size)
{
    const auto l = layout_of(1920, 1080);

    EXPECT_EQ(l.icon, 48);
    EXPECT_EQ(l.columns, 8);
    EXPECT_EQ(l.rows, 4);
    EXPECT_EQ(l.cell_width, 96);
    EXPECT_EQ(l.cell_height, 88);
}

TEST(LauncherLayoutTest, the_cells_are_centered_on_the_screen)
{
    const auto l = layout_of(1920, 1080);

    EXPECT_EQ(l.grid.x * 2 + l.grid.width, 1920);
    EXPECT_EQ(l.grid.y * 2 + l.rows * l.cell_height, 1080);
}

TEST(LauncherLayoutTest, the_cells_take_at_most_three_quarters_of_the_screen)
{
    for (const int width : {640, 1024, 1366, 1920, 2560, 3840}) {
        for (const int height : {480, 768, 1080, 1440, 2160}) {
            const auto l = layout_of(width, height);
            EXPECT_LE(l.columns * l.cell_width, width * 3 / 4)
                    << width << "x" << height;
            EXPECT_LE(l.rows * l.cell_height, height * 3 / 4)
                    << width << "x" << height;
        }
    }
}

TEST(LauncherLayoutTest, the_entry_is_in_the_middle_between_the_panel_and_cells)
{
    const Themes theme;
    const auto l = layout_of(1920, 1080);

    const int outer = l.entry.height + 2 * theme.toolkit.entry.border_width;
    const int above = l.entry.y - theme.shell.panel;
    const int under = l.grid.y - (l.entry.y + outer);
    EXPECT_NEAR(above, under, 1);
    EXPECT_GT(above, 0);
}

TEST(LauncherLayoutTest, the_entry_is_a_third_of_the_screen_wide)
{
    const Themes theme;
    const auto l = layout_of(1920, 1080);

    EXPECT_EQ(l.entry.width + 2 * theme.toolkit.entry.border_width, 640);
    EXPECT_EQ(l.entry.x * 2 + 640, 1920);
}

TEST(LauncherLayoutTest, the_entry_is_not_too_short_on_a_small_screen)
{
    const auto l = layout_of(320, 240);

    EXPECT_GT(l.entry.width, 320 / 3);
    EXPECT_LE(l.entry.width, 320);
}

TEST(LauncherLayoutTest, a_high_screen_has_more_rows_than_columns)
{
    const auto l = layout_of(1080, 1920);

    EXPECT_EQ(l.columns, 4);
    EXPECT_EQ(l.rows, 8);
}

TEST(LauncherLayoutTest, a_big_screen_gets_bigger_icons)
{
    const auto full_hd = layout_of(1920, 1080);
    const auto big = layout_of(2560, 1440);
    const auto huge = layout_of(3840, 2160);

    EXPECT_EQ(big.icon, 64);
    EXPECT_GT(huge.icon, big.icon);
    EXPECT_GT(huge.icon, full_hd.icon);
}

TEST(LauncherLayoutTest, a_vga_screen_still_has_4_x_2_cells_of_the_default_size)
{
    const auto l = layout_of(640, 480);

    EXPECT_EQ(l.icon, 48);
    EXPECT_GE(l.columns, 4);
    EXPECT_GE(l.rows, 2);
}

TEST(LauncherLayoutTest, a_small_screen_gets_smaller_icons)
{
    const auto l = layout_of(480, 320);

    EXPECT_LT(l.icon, 48);
    EXPECT_GE(l.columns, 4);
    EXPECT_GE(l.rows, 2);
}

TEST(LauncherLayoutTest, a_high_screen_needs_only_2_x_4_cells)
{
    const auto l = layout_of(480, 640);

    EXPECT_GE(l.columns, 2);
    EXPECT_GE(l.rows, 4);
}

TEST(LauncherLayoutTest, the_icons_are_between_16_and_512_pixels)
{
    EXPECT_EQ(layout_of(100, 100).icon, 16);
    EXPECT_EQ(layout_of(30000, 20000).icon, 512);
    EXPECT_EQ(layout_of(1, 1).columns, 1);
}

TEST(LauncherLayoutTest, the_icons_follow_the_scale_of_the_theme)
{
    Themes theme;
    theme.shell.scale(2);

    const auto l = tiny::wm::LauncherLayout::compute(3840, 2160, theme.shell);

    EXPECT_EQ(l.icon, 96);
    EXPECT_EQ(l.columns, 8);
    EXPECT_EQ(l.rows, 4);
}

TEST(LauncherLayoutTest, the_buttons_are_squares_beside_the_cells)
{
    const auto l = layout_of(1920, 1080);

    EXPECT_EQ(l.prev.width, l.prev.height);
    EXPECT_EQ(l.next.width, l.next.height);
    EXPECT_GT(l.prev.width, 0);
    EXPECT_LT(l.prev.x + l.prev.width, l.grid.x);
    EXPECT_GT(l.next.x, l.grid.x + l.grid.width);
}

TEST(LauncherLayoutTest, the_buttons_are_in_the_middle_of_the_free_space)
{
    const auto l = layout_of(1920, 1080);

    // the same distance from the screen and from the cells
    EXPECT_NEAR(l.prev.x, l.grid.x - (l.prev.x + l.prev.width), 1);
    EXPECT_NEAR(1920 - (l.next.x + l.next.width),
                l.next.x - (l.grid.x + l.grid.width), 1);
    // and in the middle of the height of the cells
    EXPECT_NEAR(l.prev.y * 2 + l.prev.height, 1080, 1);
    EXPECT_EQ(l.prev.y, l.next.y);
}

TEST(LauncherLayoutTest, the_layouts_differ_by_the_screen_and_the_icons)
{
    EXPECT_EQ(layout_of(1920, 1080), layout_of(1920, 1080));
    EXPECT_FALSE(layout_of(1920, 1080) == layout_of(1280, 1024));
    EXPECT_FALSE(layout_of(1920, 1080) == layout_of(3840, 2160));
}
