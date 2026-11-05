#include "wm/clock_label.h"
#include "wm/shell_theme.h"

namespace tiny::wm {

namespace {

constexpr int seconds_in_day = 24 * 60 * 60;
constexpr std::time_t start_of_year = 1704067200; // 2024-01-01 00:00 UTC

//! The width of the widest text of the format in a (leap) year and a day
int widest_text(const std::string& format, XftFont* font, ::Display* display)
{
    // the times of the day with the digits that differ in width, AM and PM
    int rv = 0;
    constexpr int times[] = {0,
                             8 * 3600 + 8 * 60,
                             11 * 3600 + 11 * 60,
                             15 * 3600 + 33 * 60,
                             19 * 3600 + 59 * 60,
                             22 * 3600 + 22 * 60};
    for (int day = 0; day < 366; ++day) {
        for (const int time : times) {
            const std::string text = ClockLabel::format_time(
                    format,
                    start_of_year + std::time_t{day} * seconds_in_day + time);
            XGlyphInfo extents;
            XftTextExtentsUtf8(display, font,
                               reinterpret_cast<const FcChar8*>(text.c_str()),
                               static_cast<int>(text.size()), &extents);
            rv = std::max(rv, static_cast<int>(extents.xOff));
        }
    }
    return rv;
}

} // namespace

ClockLabel::ClockLabel(const std::string& format_arg):
    tiny::Label(1, shell_theme.panel), format(format_arg)
{
    name = "wm_clock";
    resize(widest_text(format, get_style().get_font(), display) +
                   2 * get_padding(),
           shell_theme.panel);
}

ClockLabel::~ClockLabel() = default;

std::string ClockLabel::format_time(const std::string& format, std::time_t time)
{
    std::tm local{};
    if (localtime_r(&time, &local) == nullptr) {
        return {};
    }
    char buffer[1024];
    // 0 is also the result for an empty text, so an empty format is empty
    // the format is the setting of the user, strftime() has no way to check it
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    const size_t size =
            strftime(buffer, sizeof(buffer), format.c_str(), &local);
#pragma GCC diagnostic pop
    return {buffer, size};
}

void ClockLabel::update(Clock::time_point now)
{
    const std::string shown = format_time(format, Clock::to_time_t(now));
    if (shown != get_text()) {
        set_text(shown);
    }
}

std::chrono::milliseconds ClockLabel::until_next_minute(Clock::time_point now)
{
    using namespace std::chrono;
    const auto since_epoch =
            duration_cast<milliseconds>(now.time_since_epoch());
    const milliseconds into_minute{since_epoch.count() % 60000};
    return milliseconds{60000} - into_minute;
}

} // namespace tiny::wm
