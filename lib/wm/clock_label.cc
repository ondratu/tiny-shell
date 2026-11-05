#include <set>
#include <vector>

#include "wm/clock_label.h"
#include "wm/shell_theme.h"

namespace tiny::wm {

namespace {

constexpr int seconds_in_day = 24 * 60 * 60;
constexpr std::time_t start_of_year = 1704110400; // 2024-01-01 12:00 UTC

//! The width of the widest text of the format in a (leap) year and a day
int widest_text(const std::string& format, XftFont* font, ::Display* display)
{
    std::set<std::string> texts;
    // the times of the day with the digits that differ in width, AM and PM
    constexpr int times[] = {0,
                             8 * 3600 + 8 * 60,
                             11 * 3600 + 11 * 60,
                             15 * 3600 + 33 * 60,
                             19 * 3600 + 59 * 60,
                             22 * 3600 + 22 * 60};
    for (int day = 0; day < 366; ++day) {
        for (const int time : times) {
            texts.insert(ClockLabel::format_time(
                    format, start_of_year - 12 * 3600 +
                                    std::time_t{day} * seconds_in_day + time));
        }
    }
    int rv = 0;
    for (const std::string& text : texts) {
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, font,
                           reinterpret_cast<const FcChar8*>(text.c_str()),
                           static_cast<int>(text.size()), &extents);
        rv = std::max(rv, static_cast<int>(extents.xOff));
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
        return "";
    }
    std::vector<char> buffer(1024);
    // 0 is also the result for an empty text, so an empty format is empty
    const size_t size =
            strftime(buffer.data(), buffer.size(), format.c_str(), &local);
    return std::string(buffer.data(), size);
}

void ClockLabel::update(Clock::time_point now)
{
    const std::string text = format_time(format, Clock::to_time_t(now));
    if (text != get_text()) {
        set_text(text);
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
