#include <gtest/gtest.h>

#include "theme.h"
#include "wm/shell_theme.h"

using tiny::Style;
using tiny::wm::ShellTheme;

// A local theme is used: the global one is destroyed after the display.
class ShellThemeTest : public ::testing::Test {
  protected:
    tiny::Theme toolkit;
    ShellTheme t{toolkit};
};

TEST_F(ShellThemeTest, init_opens_fonts_of_all_styles_and_of_the_toolkit)
{
    t.init();

    EXPECT_NE(toolkit.widget.get_font(), nullptr);
    EXPECT_NE(t.button.get_font(), nullptr);
    EXPECT_NE(t.header.get_font(), nullptr);
}

TEST_F(ShellThemeTest, release_closes_all_the_fonts)
{
    t.init();

    t.release();

    EXPECT_EQ(toolkit.widget.get_font(), nullptr);
    EXPECT_EQ(t.button.get_font(), nullptr);
    EXPECT_EQ(t.header.get_font(), nullptr);
}

TEST_F(ShellThemeTest, default_window_metrics)
{
    EXPECT_EQ(t.win_header, 25);
    EXPECT_EQ(t.win_corner, 2 * t.win_border);
    EXPECT_EQ(t.win_min_height, 10 + t.win_header);
}

TEST_F(ShellThemeTest, the_styles_of_the_shell_override_the_widget_defaults)
{
    EXPECT_EQ(t.button.border_width, 0);
    EXPECT_EQ(t.button.normal_bg, 0x303436u); // the dark variant by default
    EXPECT_EQ(t.header.padding_width, 2);
    EXPECT_EQ(t.dock.padding_width, 5);
}

TEST_F(ShellThemeTest, scale_multiplies_metrics_and_styles_and_the_toolkit)
{
    t.scale(2);

    EXPECT_EQ(t.win_header, 50);
    EXPECT_EQ(t.icon, 96);
    EXPECT_EQ(t.win_corner, 2 * t.win_border);
    EXPECT_EQ(t.win_min_height, 20 + t.win_header);
    EXPECT_EQ(toolkit.widget.border_width, 2);
    EXPECT_EQ(toolkit.scale_factor, 2.0);
    EXPECT_EQ(t.header.padding_width, 4);
    EXPECT_EQ(t.dock.padding_width, 10);
    EXPECT_EQ(t.menu_item_height, 40);
    EXPECT_EQ(t.menu_separator, 16);
    EXPECT_EQ(t.popup_item_height, 60);
    EXPECT_EQ(t.menu_padding, 24);
    EXPECT_EQ(t.tray_icon, 36);
    EXPECT_EQ(t.tray_gap, 8);
    EXPECT_EQ(t.dock_dot, 8);
    EXPECT_EQ(t.dock_line, 2);
    EXPECT_EQ(t.dock_marker, 6);
    EXPECT_EQ(t.dock_drag, 12);
    EXPECT_EQ(t.launcher_cell_width, 128);
    EXPECT_EQ(t.launcher_label, 80);
    EXPECT_EQ(t.launcher_icon, 96);
    EXPECT_EQ(t.launcher_dot, 16);
    EXPECT_EQ(t.switcher_cell_width, 192);
    EXPECT_EQ(t.switcher_window_cell_width, 320);
    EXPECT_EQ(t.switcher_window_icon, 256);
    EXPECT_EQ(t.launcher_columns, 8); // a count is not scaled
    EXPECT_EQ(t.launcher_rows, 4);
}

TEST_F(ShellThemeTest, scale_is_relative_to_defaults_not_cumulative)
{
    t.scale(2);
    t.scale(1);

    EXPECT_EQ(t.win_header, 25);
    EXPECT_EQ(toolkit.widget.padding_width, 1);
    EXPECT_EQ(t.header.padding_width, 2);
}

TEST_F(ShellThemeTest, scaled_theme_opens_fonts)
{
    t.scale(1.5);
    t.init();

    EXPECT_NE(t.header.get_font(), nullptr);
}

TEST_F(ShellThemeTest, the_font_is_set_before_init)
{
    t.set_font("DejaVu Sans Mono");
    t.set_font_size(10);
    t.set_font_style("Bold");
    t.init();

    // fontconfig gives the closest font when the family is not installed
    EXPECT_NE(toolkit.widget.get_font(), nullptr);
    EXPECT_NE(toolkit.entry.get_font(), nullptr);
    EXPECT_NE(toolkit.list.get_font(), nullptr);
    EXPECT_NE(t.button.get_font(), nullptr);
    EXPECT_NE(t.header.get_font(), nullptr);
}

TEST_F(ShellThemeTest, the_variant_is_set_to_the_toolkit_too)
{
    t.set_variant(tiny::Variant::Light);

    EXPECT_EQ(toolkit.get_variant(), tiny::Variant::Light);
    EXPECT_GT(toolkit.widget.normal_bg, 0xF0F0F0u);
    // the lines and marks of the dock are visible on a light panel
    EXPECT_GT(t.dock_separator.normal_bg, 0x808080u);
    EXPECT_NE(t.dock_mark.normal_bg, 0x729FCFu);
    // the title bar is light in both variants
    EXPECT_EQ(t.header.normal_bg, 0xF8F8F8u);

    t.set_variant(tiny::Variant::Dark);

    EXPECT_EQ(toolkit.widget.normal_bg, Style::BLACK);
    EXPECT_EQ(t.dock_separator.normal_bg, 0x555753u);
    EXPECT_EQ(t.dock_mark.normal_bg, 0x729FCFu);
}

TEST_F(ShellThemeTest, the_title_bar_and_the_buttons_follow_the_variant)
{
    t.set_variant(tiny::Variant::Light);
    EXPECT_EQ(t.header.normal_bg, 0xF8F8F8u);
    EXPECT_EQ(t.button.normal_bg, 0xF8F8F8u);

    t.set_variant(tiny::Variant::Dark);
    EXPECT_EQ(t.header.normal_bg, 0x303436u);
    EXPECT_EQ(t.button.normal_bg, 0x303436u);
}

TEST_F(ShellThemeTest, the_panel_and_the_dock_follow_the_theme_by_default)
{
    t.set_variant(tiny::Variant::Light);
    EXPECT_EQ(t.panel_variant(), tiny::Variant::Light);
    EXPECT_EQ(t.dock_variant(), tiny::Variant::Light);
    EXPECT_EQ(t.panel_widgets.normal_bg, 0xF6F5F4u);

    t.set_variant(tiny::Variant::Dark);
    EXPECT_EQ(t.panel_variant(), tiny::Variant::Dark);
    EXPECT_EQ(t.panel_widgets.normal_bg, 0x000000u);
    EXPECT_EQ(t.dock.normal_bg, 0x000000u);
}

TEST_F(ShellThemeTest, the_panel_and_the_dock_can_keep_a_look_of_their_own)
{
    t.set_variant(tiny::Variant::Dark);
    t.set_looks(tiny::Look::Light, tiny::Look::Auto);
    EXPECT_EQ(t.panel_widgets.normal_bg, 0xF6F5F4u); // light in a dark theme
    EXPECT_EQ(t.panel_separator.normal_bg, 0xC0BFBCu);
    EXPECT_EQ(t.dock_widgets.normal_bg, 0x000000u); // the dock follows

    t.set_variant(tiny::Variant::Light);
    t.set_looks(tiny::Look::Auto, tiny::Look::Dark);
    EXPECT_EQ(t.panel_widgets.normal_bg, 0xF6F5F4u);
    EXPECT_EQ(t.dock.normal_bg, 0x000000u); // dark in a light theme
    EXPECT_EQ(t.dock_widgets.normal_bg, 0x000000u);
    EXPECT_EQ(t.dock_separator.normal_bg, 0x555753u);
}

TEST_F(ShellThemeTest, a_change_of_the_variant_keeps_the_looks)
{
    t.set_looks(tiny::Look::Dark, tiny::Look::Light);
    t.set_variant(tiny::Variant::Light);
    t.set_variant(tiny::Variant::Dark);
    EXPECT_EQ(t.get_panel_look(), tiny::Look::Dark);
    EXPECT_EQ(t.get_dock_look(), tiny::Look::Light);
    EXPECT_EQ(t.dock_variant(), tiny::Variant::Light);
}

TEST_F(ShellThemeTest, a_change_of_a_look_makes_hidden_widgets_take_the_colors)
{
    const unsigned before = toolkit.generation;
    t.set_looks(tiny::Look::Light, tiny::Look::Light);
    EXPECT_GT(toolkit.generation, before);
}
