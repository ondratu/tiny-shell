#include <gtest/gtest.h>

#include "app/app_colors.h"
#include "display.h"
#include "wm/xsettings.h"

using tiny::Variant;
using tiny::app::app_colors;

TEST(AppColorsTest, the_light_variant_has_dark_text_on_a_light_window)
{
    const auto colors = app_colors(Variant::Light);

    EXPECT_EQ(colors.background, 0xF8F8F8u);
    EXPECT_EQ(colors.text, 0x2E3436u);
}

TEST(AppColorsTest, the_dark_variant_has_light_text_on_a_dark_window)
{
    const auto colors = app_colors(Variant::Dark);

    EXPECT_LT(colors.background, 0x404040u);
    EXPECT_GT(colors.text, 0xC0C0C0u);
    EXPECT_NE(colors.field, colors.background);
    EXPECT_NE(colors.selected, colors.background);
}

TEST(AppColorsTest, a_program_looks_as_configured_without_a_manager_of_settings)
{
    ::Display* const display = tiny::get_display();
    const int screen = XDefaultScreen(display);
    if (tiny::wm::running_variant(display, screen)) {
        GTEST_SKIP() << "a manager of settings runs on this display";
    }

    EXPECT_EQ(tiny::app::variant_in_use(display, screen, Variant::Light),
              Variant::Light);
    EXPECT_EQ(tiny::app::variant_in_use(display, screen, Variant::Dark),
              Variant::Dark);
}
