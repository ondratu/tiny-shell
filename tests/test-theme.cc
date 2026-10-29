#include <gtest/gtest.h>

#include "theme.h"

using tiny::Style;

static constexpr uint8_t NORMAL = uint8_t(Style::State::Normal);
static constexpr uint8_t SELECTED = uint8_t(Style::State::Selected);
static constexpr uint8_t HOVER = uint8_t(Style::State::Hover);
static constexpr uint8_t FOCUS = uint8_t(Style::State::Focus);
static constexpr uint8_t PRESSED = uint8_t(Style::State::Pressed);
static constexpr uint8_t DISABLED = uint8_t(Style::State::Disabled);

class StyleTest: public ::testing::Test {
  protected:
    Style style = tiny::theme.widget;
};

TEST(StyleDefaults, font_is_null_before_init_and_destructor_is_safe)
{
    Style style;    // destructor must not close an uninitialized font

    EXPECT_EQ(style.get_font(), nullptr);
}

TEST_F(StyleTest, normal_state_uses_normal_colors)
{
    EXPECT_EQ(style.get_bg(NORMAL), style.normal_bg);
    EXPECT_EQ(style.get_fg(NORMAL), style.normal_fg);
    EXPECT_EQ(style.get_br(NORMAL), style.normal_br);
}

TEST_F(StyleTest, hover_overrides_normal)
{
    EXPECT_EQ(style.get_bg(NORMAL|HOVER), style.hover_bg);
    EXPECT_EQ(style.get_fg(NORMAL|HOVER), style.hover_fg);
}

TEST_F(StyleTest, inherited_color_falls_back_to_normal)
{
    ASSERT_EQ(style.hover_br, Style::INHERITED);

    EXPECT_EQ(style.get_br(NORMAL|HOVER), style.normal_br);
}

TEST_F(StyleTest, selected_overrides_normal)
{
    EXPECT_EQ(style.get_bg(NORMAL|SELECTED), style.selected_bg);
    // selected_fg is inherited
    EXPECT_EQ(style.get_fg(NORMAL|SELECTED), style.normal_fg);
}

TEST_F(StyleTest, hover_overrides_selected)
{
    EXPECT_EQ(style.get_bg(NORMAL|SELECTED|HOVER), style.hover_bg);
}

TEST_F(StyleTest, inherited_state_keeps_weaker_state_color)
{
    // focus_bg is inherited, so hover color must stay
    ASSERT_EQ(style.focus_bg, Style::INHERITED);

    EXPECT_EQ(style.get_bg(NORMAL|HOVER|FOCUS), style.hover_bg);
}

TEST_F(StyleTest, state_priority_focus_then_pressed)
{
    style.focus_bg = 0x111111;
    style.pressed_bg = 0x222222;

    EXPECT_EQ(style.get_bg(NORMAL|HOVER|FOCUS), 0x111111u);
    EXPECT_EQ(style.get_bg(NORMAL|HOVER|FOCUS|PRESSED), 0x222222u);
}

TEST_F(StyleTest, disabled_overrides_everything)
{
    style.pressed_fg = 0x222222;

    EXPECT_EQ(style.get_fg(NORMAL|HOVER|PRESSED|DISABLED), style.disable_fg);
}

TEST_F(StyleTest, disabled_inherited_color_uses_normal)
{
    ASSERT_EQ(style.disable_bg, Style::INHERITED);

    EXPECT_EQ(style.get_bg(NORMAL|HOVER|DISABLED), style.normal_bg);
}

TEST_F(StyleTest, xft_color_is_uppercase_hex_with_hash)
{
    style.normal_fg = 0xA8B9CA;
    EXPECT_EQ(style.get_xft_fg(NORMAL), "#A8B9CA");

    style.normal_fg = 0x00000F;
    EXPECT_EQ(style.get_xft_fg(NORMAL), "#00000F");
}

TEST_F(StyleTest, xft_color_follows_state)
{
    EXPECT_EQ(style.get_xft_fg(NORMAL|HOVER), "#F3F3F3");
}

TEST_F(StyleTest, init_opens_font)
{
    EXPECT_EQ(style.get_font(), nullptr);

    style.init();

    EXPECT_NE(style.get_font(), nullptr);
}

TEST(StyleDefaults, delete_through_base_pointer_closes_font)
{
    Style *style = new Style(tiny::theme.widget);
    style->init();
    ASSERT_NE(style->get_font(), nullptr);

    delete style;   // must close the font while the display is still open
}

// A local Theme is used: the global one is destroyed after the display.
TEST(ThemeTest, init_opens_fonts_of_all_styles)
{
    tiny::Theme theme;

    theme.init();

    EXPECT_NE(theme.widget.get_font(), nullptr);
    EXPECT_NE(theme.wm_button.get_font(), nullptr);
    EXPECT_NE(theme.wm_header.get_font(), nullptr);
}

TEST(ThemeTest, default_window_metrics)
{
    const tiny::Theme &t = tiny::theme;

    EXPECT_EQ(t.wm_win_header, 25u);
    EXPECT_EQ(t.wm_win_corner, 2 * t.wm_win_border);
    EXPECT_EQ(t.wm_win_min_height, 10 + t.wm_win_header);
}

TEST(ThemeTest, widget_style_defaults)
{
    EXPECT_EQ(tiny::theme.widget.border_width, 1u);
    EXPECT_EQ(tiny::theme.widget.padding_width, 1u);
    EXPECT_EQ(tiny::theme.widget.normal_bg, Style::BLACK);
}

TEST(ThemeTest, wm_styles_override_widget_defaults)
{
    EXPECT_EQ(tiny::theme.wm_button.border_width, 0u);
    EXPECT_EQ(tiny::theme.wm_button.normal_bg, 0xF8F8F8u);
    EXPECT_EQ(tiny::theme.wm_header.padding_width, 2u);
    EXPECT_EQ(tiny::theme.wm_dock.padding_width, 5u);
}
