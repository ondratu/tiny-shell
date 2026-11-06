#include <stdexcept>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include <gtest/gtest.h>

#include "display.h"
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

TEST(StyleDefaultsTest, a_font_of_a_display_that_is_gone_is_not_closed)
{
    auto* style = new Style(tiny::theme.widget);
    style->init();
    ASSERT_NE(style->get_font(), nullptr);

    // the globals are destroyed in an order nobody chooses: the display can be
    // closed before the style is
    tiny::Display::init();

    delete style; // must not touch the closed display
}

TEST(StyleDefaultsTest, init_again_replaces_the_font)
{
    Style style;
    style.init();
    style.init();

    EXPECT_NE(style.get_font(), nullptr);
}

// A local Theme is used: the global one is destroyed after the display.
TEST(ThemeTest, init_opens_fonts_of_all_styles)
{
    tiny::Theme theme;

    theme.init();

    EXPECT_NE(theme.widget.get_font(), nullptr);
    EXPECT_NE(theme.entry.get_font(), nullptr);
    EXPECT_NE(theme.list.get_font(), nullptr);
}

TEST(ThemeTest, widget_style_defaults)
{
    EXPECT_EQ(tiny::theme.widget.border_width, 1u);
    EXPECT_EQ(tiny::theme.widget.padding_width, 1u);
    EXPECT_EQ(tiny::theme.widget.normal_bg, Style::BLACK);
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

    EXPECT_EQ(t.widget.border_width, 2u);
    EXPECT_EQ(t.entry.padding_width, 12u);
    EXPECT_EQ(t.entry.border_width, 2u);
    EXPECT_EQ(t.list.padding_width, 12);
}

TEST(ThemeScaleTest, scale_is_relative_to_defaults_not_cumulative)
{
    tiny::Theme t;

    t.scale(2);
    t.scale(1);

    EXPECT_EQ(t.widget.padding_width, 1u);
}

TEST(ThemeScaleTest, scaled_theme_opens_fonts)
{
    tiny::Theme t;

    t.scale(1.5);
    t.init();

    EXPECT_NE(t.widget.get_font(), nullptr);
}

TEST(ThemeScaleTest, the_scale_is_one_until_it_is_set)
{
    tiny::Theme t;

    EXPECT_EQ(t.scale_factor, 1.0);
    t.scale(2);
    EXPECT_EQ(t.scale_factor, 2.0);
}

TEST(ThemeFontTest, xft_name_has_the_family_the_size_and_the_style)
{
    EXPECT_EQ(tiny::xft_name("Cantarell", 12), "Cantarell-12");
    EXPECT_EQ(tiny::xft_name("Cantarell", 10.5, ":bold"),
              "Cantarell-10.5:bold");
}

TEST(ThemeFontTest, xft_name_escapes_what_fontconfig_would_read)
{
    EXPECT_EQ(tiny::xft_name("A-B:C", 9), "A\\-B\\:C-9");
    EXPECT_EQ(tiny::xft_name("D\\E", 9), "D\\\\E-9");
    EXPECT_EQ(tiny::xft_name("Noto Sans Mono", 9), "Noto Sans Mono-9");
}

TEST(ThemeFontTest, the_font_of_the_theme_is_set_before_init)
{
    tiny::Theme t;

    t.set_font("DejaVu Sans Mono");
    t.init();

    // fontconfig gives the closest font when the family is not installed
    EXPECT_NE(t.widget.get_font(), nullptr);
    EXPECT_NE(t.entry.get_font(), nullptr);
    EXPECT_NE(t.list.get_font(), nullptr);
}

TEST(ThemeFontTest, a_family_with_special_characters_still_opens_a_font)
{
    tiny::Theme t;

    t.set_font("No-Such: Family, 1");
    t.init();

    EXPECT_NE(t.widget.get_font(), nullptr);
}

TEST(ThemeFontTest, xft_style_asks_for_a_style_by_its_name)
{
    EXPECT_EQ(tiny::xft_style(""), "");
    EXPECT_EQ(tiny::xft_style("Bold Italic"), ":style=Bold Italic");
    EXPECT_EQ(tiny::xft_style("A-B,C"), ":style=A\\-B\\,C");
}

TEST(ThemeFontTest, the_size_of_the_font_is_set_before_the_scale)
{
    tiny::Theme t;

    t.set_font_size(10);
    t.scale(2);
    t.init();

    EXPECT_NE(t.widget.get_font(), nullptr);
    // 10 points at the scale 2 is 20 points: the font is higher than the one
    // of 10 points
    tiny::Theme small;
    small.set_font_size(10);
    small.scale(1);
    small.init();
    EXPECT_GT(t.widget.get_font()->height, small.widget.get_font()->height);
}

TEST(ThemeFontTest, the_style_is_for_all_the_styles)
{
    tiny::Theme t;

    t.set_font_style("Bold");
    t.init();

    EXPECT_NE(t.widget.get_font(), nullptr);
    EXPECT_NE(t.entry.get_font(), nullptr);
    EXPECT_NE(t.list.get_font(), nullptr);
}

TEST(ThemeVariantTest, names_of_the_variants_are_read_in_any_case)
{
    tiny::Variant v = tiny::Variant::Dark;

    EXPECT_TRUE(tiny::variant_from_name("Light", v));
    EXPECT_EQ(v, tiny::Variant::Light);
    EXPECT_TRUE(tiny::variant_from_name("DARK", v));
    EXPECT_EQ(v, tiny::Variant::Dark);
    EXPECT_FALSE(tiny::variant_from_name("blue", v));
    EXPECT_FALSE(tiny::variant_from_name("", v));
    EXPECT_EQ(v, tiny::Variant::Dark) << "a wrong name leaves it as it was";
    EXPECT_STREQ(tiny::variant_name(tiny::Variant::Light), "light");
    EXPECT_STREQ(tiny::variant_name(tiny::Variant::Dark), "dark");
}

TEST(ThemeVariantTest, the_theme_is_dark_until_it_is_changed)
{
    const tiny::Theme t;

    EXPECT_EQ(t.get_variant(), tiny::Variant::Dark);
    EXPECT_EQ(t.widget.normal_bg, Style::BLACK);
}

TEST(ThemeVariantTest, the_light_variant_has_light_widgets)
{
    tiny::Theme t;
    t.scale(2);

    t.set_variant(tiny::Variant::Light);

    EXPECT_EQ(t.get_variant(), tiny::Variant::Light);
    EXPECT_GT(t.widget.normal_bg, 0xF0F0F0u);
    EXPECT_LT(t.widget.normal_fg, 0x404040u);
    EXPECT_EQ(t.entry.normal_bg, 0xFFFFFFu);
    EXPECT_EQ(t.entry.focus_br, 0x729FCFu);
    EXPECT_GT(t.list.selected_bg, 0xC0C0C0u);
    // the metrics stay
    EXPECT_EQ(t.widget.border_width, 2u);
    EXPECT_EQ(t.entry.padding_width, 12u);
}

TEST(ThemeVariantTest, going_back_to_dark_restores_the_colors)
{
    tiny::Theme t;
    const Style original = t.widget;
    const tiny::EntryStyle entry = t.entry;

    t.set_variant(tiny::Variant::Light);
    t.set_variant(tiny::Variant::Dark);

    EXPECT_EQ(t.widget.normal_bg, original.normal_bg);
    EXPECT_EQ(t.widget.hover_fg, original.hover_fg);
    EXPECT_EQ(t.widget.selected_bg, original.selected_bg);
    EXPECT_EQ(t.entry.normal_bg, entry.normal_bg);
    EXPECT_EQ(t.entry.normal_br, entry.normal_br);
}

TEST(ThemeFontTest, a_list_of_families_is_kept_as_a_list)
{
    EXPECT_EQ(tiny::xft_family("DejaVu Sans,sans"), "DejaVu Sans,sans");
    EXPECT_EQ(tiny::xft_name("DejaVu Sans,sans", 12), "DejaVu Sans,sans-12");
}

TEST(ThemeFontTest, the_blanks_and_empty_names_of_a_list_are_dropped)
{
    EXPECT_EQ(tiny::xft_family(" A , ,B ,"), "A,B");
    EXPECT_EQ(tiny::xft_family(""), "");
}

TEST(ThemeFontTest, every_name_of_a_list_is_escaped)
{
    EXPECT_EQ(tiny::xft_family("A-B,C:D"), "A\\-B,C\\:D");
}

TEST(ThemeFontTest, the_pixel_name_has_the_size_in_pixels)
{
    EXPECT_EQ(tiny::xft_pixel_name("DejaVu Sans,sans", 14),
              "DejaVu Sans,sans:pixelsize=14");
}
