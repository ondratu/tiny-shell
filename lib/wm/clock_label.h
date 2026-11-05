#pragma once

#include <chrono>
#include <ctime>
#include <string>

#include "label.h"

namespace tiny::wm {

/**
 * @short The date and the time in the panel.
 *
 * The text is made by `strftime()` from the format, so the names of the days
 * and the order of the date follow `LC_TIME` (the program calls
 * `setlocale(LC_TIME, "")`). The width is fixed at the widest text that the
 * format makes in a year, so the clock does not move the other widgets. The
 * clock shows minutes: `update()` is called at the start of every minute.
 */
class ClockLabel : public tiny::Label {
  public:
    using Clock = std::chrono::system_clock;

    explicit ClockLabel(const std::string& format);

    ~ClockLabel() override;

    //! Show the time `now` (nothing is drawn when the text is the same)
    void update(Clock::time_point now = Clock::now());

    //! The time in the format, in the local time zone
    static std::string format_time(const std::string& format, std::time_t time);

    //! The time to the start of the next minute (at least one millisecond)
    static std::chrono::milliseconds
    until_next_minute(Clock::time_point now = Clock::now());

  private:
    std::string format;
};

} // namespace tiny::wm
