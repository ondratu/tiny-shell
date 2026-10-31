#include <stdexcept>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include <gtest/gtest.h>

#include "theme.h"

using tiny::Style;

static constexpr uint8_t NORMAL = static_cast<uint8_t>(Style::State::Normal);
static constexpr uint8_t SELECTED =
        static_cast<uint8_t>(Style::State::Selected);
static constexpr uint8_t HOVER = static_cast<uint8_t>(Style::State::Hover);
static constexpr uint8_t FOCUS = static_cast<uint8_t>(Style::State::Focus);
static constexpr uint8_t PRESSED = static_cast<uint8_t>(Style::State::Pressed);
static constexpr uint8_t DISABLED =
        static_cast<uint8_t>(Style::State::Disabled);

class StyleTest : public ::testing::Test {
  protected:
    Style style = tiny::theme.widget;
};

TEST(StyleDefaultsTest, font_is_null_before_init_and_destructor_is_safe)
{
    Style style; // destructor must not close an uninitialized font

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
    EXPECT_EQ(style.get_bg(NORMAL | HOVER), style.hover_bg);
    EXPECT_EQ(style.get_fg(NORMAL | HOVER), style.hover_fg);
}

TEST_F(StyleTest, inherited_color_falls_back_to_normal)
{
    ASSERT_EQ(style.hover_br, Style::INHERITED);

    EXPECT_EQ(style.get_br(NORMAL | HOVER), style.normal_br);
}

TEST_F(StyleTest, selected_overrides_normal)
{
    EXPECT_EQ(style.get_bg(NORMAL | SELECTED), style.selected_bg);
    // selected_fg is inherited
    EXPECT_EQ(style.get_fg(NORMAL | SELECTED), style.normal_fg);
}

TEST_F(StyleTest, hover_overrides_selected)
{
    EXPECT_EQ(style.get_bg(NORMAL | SELECTED | HOVER), style.hover_bg);
}

TEST_F(StyleTest, inherited_state_keeps_weaker_state_color)
{
    // focus_bg is inherited, so hover color must stay
    ASSERT_EQ(style.focus_bg, Style::INHERITED);

    EXPECT_EQ(style.get_bg(NORMAL | HOVER | FOCUS), style.hover_bg);
}

TEST_F(StyleTest, state_priority_focus_then_pressed)
{
    style.focus_bg = 0x111111;
    style.pressed_bg = 0x222222;

    EXPECT_EQ(style.get_bg(NORMAL | HOVER | FOCUS), 0x111111u);
    EXPECT_EQ(style.get_bg(NORMAL | HOVER | FOCUS | PRESSED), 0x222222u);
}

TEST_F(StyleTest, disabled_overrides_everything)
{
    style.pressed_fg = 0x222222;

    EXPECT_EQ(style.get_fg(NORMAL | HOVER | PRESSED | DISABLED),
              style.disable_fg);
}

TEST_F(StyleTest, disabled_inherited_color_uses_normal)
{
    ASSERT_EQ(style.disable_bg, Style::INHERITED);

    EXPECT_EQ(style.get_bg(NORMAL | HOVER | DISABLED), style.normal_bg);
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
    EXPECT_EQ(style.get_xft_fg(NORMAL | HOVER), "#F3F3F3");
}

TEST_F(StyleTest, init_opens_font)
{
    EXPECT_EQ(style.get_font(), nullptr);

    style.init();

    EXPECT_NE(style.get_font(), nullptr);
}

namespace {
// Fontconfig falls back to some font for any pattern it can parse, and
// what it can parse differs between versions. Without any font there is
// nothing to fall back to, so run init() in a fresh process (fontconfig
// reads its configuration only once) with a configuration that has none.
[[noreturn]] void init_without_fonts()
{
    const auto conf =
            std::filesystem::temp_directory_path() /
            ("tiny-empty-fonts-" + std::to_string(getpid()) + ".conf");
    std::ofstream(conf) << "<?xml version=\"1.0\"?>\n"
                           "<fontconfig></fontconfig>\n";
    setenv("FONTCONFIG_FILE", conf.c_str(), 1);

    Style style;
    int result = 1;
    try {
        style.init();
    } catch (const std::runtime_error&) {
        result = style.get_font() == nullptr ? 0 : 2;
    }
    std::filesystem::remove(conf);
    std::exit(result);
}
} // namespace

TEST(StyleDefaultsTest, init_throws_when_font_cannot_be_opened)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_EXIT(init_without_fonts(), ::testing::ExitedWithCode(0), "");
}

TEST(StyleDefaultsTest, delete_through_base_pointer_closes_font)
{
    auto* style = new Style(tiny::theme.widget);
    style->init();
    ASSERT_NE(style->get_font(), nullptr);

    delete style; // must close the font while the display is still open
}

// A local Theme is used: the global one is destroyed after the display.
TEST(ThemeTest, init_opens_fonts_of_all_styles)
{
    tiny::Theme theme;

    theme.init();

    EXPECT_NE(theme.widget.get_font(), nullptr);
    EXPECT_NE(theme.wm_button.get_font(), nullptr);
    EXPECT_NE(theme.wm_header.get_font(), nullptr);
    EXPECT_NE(theme.entry.get_font(), nullptr);
    EXPECT_NE(theme.list.get_font(), nullptr);
}

TEST(ThemeTest, default_window_metrics)
{
    const tiny::Theme& t = tiny::theme;

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

TEST(ThemeTest, entry_style_is_dark_gray_with_a_highlighted_focus)
{
    const auto normal = static_cast<uint8_t>(Style::State::Normal);
    const auto focus = static_cast<uint8_t>(
            normal | static_cast<uint8_t>(Style::State::Focus));
    const tiny::EntryStyle& entry = tiny::theme.entry;

    EXPECT_EQ(entry.get_bg(normal), 0x2E3436u);
    EXPECT_EQ(entry.get_bg(focus), 0x2E3436u);
    EXPECT_NE(entry.get_br(focus), entry.get_br(normal));
    EXPECT_EQ(entry.border_width, 1u);
}

TEST(ThemeTest, list_style_has_no_border_and_a_lighter_selected_row)
{
    const auto normal = static_cast<uint8_t>(Style::State::Normal);
    const auto selected = static_cast<uint8_t>(
            normal | static_cast<uint8_t>(Style::State::Selected));
    const tiny::ListStyle& list = tiny::theme.list;

    EXPECT_EQ(list.border_width, 0);
    EXPECT_NE(list.get_bg(selected), list.get_bg(normal));
    EXPECT_NE(list.get_fg(selected), list.get_fg(normal));
}

TEST(ThemeScaleTest, scale_multiplies_metrics_and_styles)
{
    tiny::Theme t;

    t.scale(2);

    EXPECT_EQ(t.wm_win_header, 50u);
    EXPECT_EQ(t.wm_icon, 96u);
    EXPECT_EQ(t.wm_win_corner, 2 * t.wm_win_border);
    EXPECT_EQ(t.wm_win_min_height, 20 + t.wm_win_header);
    EXPECT_EQ(t.widget.border_width, 2u);
    EXPECT_EQ(t.wm_header.padding_width, 4u);
    EXPECT_EQ(t.wm_dock.padding_width, 10u);
    EXPECT_EQ(t.entry.padding_width, 12u);
    EXPECT_EQ(t.entry.border_width, 2u);
    EXPECT_EQ(t.list.padding_width, 12);
    EXPECT_EQ(t.wm_launcher_width, 840);
    EXPECT_EQ(t.wm_launcher_icon, 64);
    EXPECT_EQ(t.wm_launcher_rows, 8);
}

TEST(ThemeScaleTest, scale_is_relative_to_defaults_not_cumulative)
{
    tiny::Theme t;

    t.scale(2);
    t.scale(1);

    EXPECT_EQ(t.wm_win_header, 25u);
    EXPECT_EQ(t.widget.padding_width, 1u);
    EXPECT_EQ(t.wm_header.padding_width, 2u);
}

TEST(ThemeScaleTest, scaled_theme_opens_fonts)
{
    tiny::Theme t;

    t.scale(1.5);
    t.init();

    EXPECT_NE(t.widget.get_font(), nullptr);
    EXPECT_NE(t.wm_header.get_font(), nullptr);
}

TEST(ThemeScaleTest, env_scale_factor_parsing)
{
    unsetenv("TINY_SCALE_FACTOR");
    EXPECT_EQ(tiny::Theme::env_scale_factor(), 1.0);

    setenv("TINY_SCALE_FACTOR", "2", 1);
    EXPECT_EQ(tiny::Theme::env_scale_factor(), 2.0);

    setenv("TINY_SCALE_FACTOR", "1.5", 1);
    EXPECT_EQ(tiny::Theme::env_scale_factor(), 1.5);

    for (const char* bad : {"", "abc", "0", "-2", "2x", "1000"}) {
        setenv("TINY_SCALE_FACTOR", bad, 1);
        EXPECT_EQ(tiny::Theme::env_scale_factor(), 1.0) << bad;
    }
    unsetenv("TINY_SCALE_FACTOR");
}
