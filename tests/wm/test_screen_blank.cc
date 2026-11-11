#include <gtest/gtest.h>

#include <X11/Xlib.h>

#include "wm/screen_blank.h"

using tiny::wm::ScreenBlank;

namespace {

class ScreenBlankTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        display = XOpenDisplay(nullptr);
        ASSERT_NE(display, nullptr);
    }

    void TearDown() override
    {
        if (display != nullptr) {
            XCloseDisplay(display);
        }
    }

    XWindowAttributes attributes(::Window window) const
    {
        XSync(display, False);
        XWindowAttributes attrs = {};
        XGetWindowAttributes(display, window, &attrs);
        return attrs;
    }

    ::Display* display = nullptr;
};

} // namespace

TEST_F(ScreenBlankTest, nothing_is_made_before_the_first_show)
{
    const ScreenBlank blank(display);
    EXPECT_FALSE(blank.shown());
    EXPECT_EQ(blank.get_window(), static_cast<::Window>(None));
}

TEST_F(ScreenBlankTest, it_covers_the_whole_screen_and_can_be_hidden)
{
    ScreenBlank blank(display);
    blank.show();
    ASSERT_TRUE(blank.shown());
    const int screen = DefaultScreen(display);
    const XWindowAttributes shown = attributes(blank.get_window());
    EXPECT_EQ(shown.map_state, IsViewable);
    EXPECT_EQ(shown.width, DisplayWidth(display, screen));
    EXPECT_EQ(shown.height, DisplayHeight(display, screen));
    EXPECT_TRUE(shown.override_redirect);

    blank.hide();
    EXPECT_FALSE(blank.shown());
    EXPECT_EQ(attributes(blank.get_window()).map_state, IsUnmapped);

    blank.show(); // again, with the same window
    EXPECT_EQ(attributes(blank.get_window()).map_state, IsViewable);
}

TEST_F(ScreenBlankTest, hiding_what_was_never_shown_is_fine)
{
    ScreenBlank blank(display);
    blank.hide();
    EXPECT_FALSE(blank.shown());
}

TEST_F(ScreenBlankTest, the_window_goes_away_with_the_object)
{
    ::Window window = None;
    {
        ScreenBlank blank(display);
        blank.show();
        window = blank.get_window();
    }
    XSync(display, False);
    XWindowAttributes attrs = {};
    XErrorHandler old =
            XSetErrorHandler([](::Display*, XErrorEvent*) { return 0; });
    const Status found = XGetWindowAttributes(display, window, &attrs);
    XSync(display, False);
    XSetErrorHandler(old);
    EXPECT_EQ(found, 0);
}
