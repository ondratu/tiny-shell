#include "wm_helpers.h"

#include <clocale>
#include <cstdlib>

#include "wm/clock_label.h"

using tiny::wm::ClockLabel;
using namespace std::chrono;

namespace {

// 2024-03-05 10:07:30 UTC
const ClockLabel::Clock::time_point moment =
        ClockLabel::Clock::from_time_t(1709633250);

// The tests do not depend on the time zone of the machine
class ClockLabelTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        const char* old = getenv("TZ");
        had_tz = old != nullptr;
        if (had_tz) {
            old_tz = old;
        }
        setenv("TZ", "UTC", 1);
        tzset();
    }

    void TearDown() override
    {
        if (had_tz) {
            setenv("TZ", old_tz.c_str(), 1);
        } else {
            unsetenv("TZ");
        }
        tzset();
    }

    const WidgetFont font;

  private:
    bool had_tz = false;
    std::string old_tz;
};

} // namespace

TEST_F(ClockLabelTest, format_time_uses_the_format_and_the_local_time)
{
    const std::time_t t = ClockLabel::Clock::to_time_t(moment);

    EXPECT_EQ(ClockLabel::format_time("%H:%M", t), "10:07");
    EXPECT_EQ(ClockLabel::format_time("%Y-%m-%d %R", t), "2024-03-05 10:07");
    EXPECT_EQ(ClockLabel::format_time("", t), "");
}

TEST_F(ClockLabelTest, format_time_follows_the_locale_of_the_time)
{
    const std::time_t t = ClockLabel::Clock::to_time_t(moment);
    const std::string saved = setlocale(LC_TIME, nullptr);
    if (setlocale(LC_TIME, "cs_CZ.UTF-8") == nullptr) {
        GTEST_SKIP() << "the locale cs_CZ.UTF-8 is not installed";
    }

    EXPECT_EQ(ClockLabel::format_time("%A", t), "úterý");

    setlocale(LC_TIME, saved.c_str());
    EXPECT_EQ(ClockLabel::format_time("%A", t), "Tuesday");
}

TEST_F(ClockLabelTest, a_text_that_does_not_fit_the_buffer_is_cut_to_empty)
{
    const std::time_t t = ClockLabel::Clock::to_time_t(moment);

    EXPECT_EQ(ClockLabel::format_time(std::string(2000, 'x'), t), "");
}

TEST_F(ClockLabelTest, until_next_minute_counts_to_the_start_of_a_minute)
{
    EXPECT_EQ(ClockLabel::until_next_minute(moment), 30s);
    EXPECT_EQ(ClockLabel::until_next_minute(moment + 500ms), 29500ms);
    // right at the start the next one is a whole minute away, never zero
    EXPECT_EQ(ClockLabel::until_next_minute(moment - 30s), 60s);
    EXPECT_EQ(ClockLabel::until_next_minute(moment + 29s + 999ms), 1ms);
}

TEST_F(ClockLabelTest, update_shows_the_time)
{
    ClockLabel clock("%H:%M");

    clock.update(moment);

    EXPECT_EQ(clock.get_text(), "10:07");
    clock.update(moment + 60s);
    EXPECT_EQ(clock.get_text(), "10:08");
}

TEST_F(ClockLabelTest, is_as_high_as_the_panel_and_as_wide_as_the_widest_text)
{
    ClockLabel narrow("%H");
    ClockLabel wide("%A %d. %B %Y %H:%M");

    EXPECT_EQ(narrow.get_height(), tiny::wm::shell_theme.panel);
    EXPECT_GT(narrow.get_width(), 2 * narrow.get_padding());
    EXPECT_GT(wide.get_width(), narrow.get_width());
}

TEST_F(ClockLabelTest, the_width_holds_every_day_of_the_year)
{
    ClockLabel clock("%A %B");
    XGlyphInfo extents;
    XftFont* font = tiny::theme.widget.get_font();

    for (int day = 0; day < 366; day += 7) {
        const std::string text = ClockLabel::format_time(
                "%A %B", 1704110400 + std::time_t{day} * 86400);
        XftTextExtentsUtf8(tiny::get_display(), font,
                           reinterpret_cast<const FcChar8*>(text.c_str()),
                           static_cast<int>(text.size()), &extents);
        EXPECT_LE(extents.xOff + 2 * clock.get_padding(), clock.get_width())
                << text;
    }
}

TEST_F(ClockLabelTest, an_empty_format_makes_an_empty_clock)
{
    ClockLabel clock("");

    clock.update(moment);

    EXPECT_EQ(clock.get_text(), "");
}
