#pragma once

#include <chrono>
#include <functional>
#include <map>

namespace tiny {

/**
 * @short Timers of the event loop.
 *
 * The loop asks `timeout_ms()` for the time it may sleep in `poll()` and calls
 * `run_due()` after it. It works with a monotonic clock, so a change of the
 * wall clock does not move the timers. The functions that take `now` exist for
 * the tests.
 */
class Timers {
  public:
    using Clock = std::chrono::steady_clock;
    using Id = unsigned long;

    //! Call `function` once, after `delay`; returns the id for `cancel()`
    Id after(std::chrono::milliseconds delay, std::function<void()> function,
             Clock::time_point now = Clock::now());

    /**
     * @short Call `function` every `period`, the first time after `period`.
     *
     * A period of zero is made one millisecond, so the loop cannot spin. A
     * timer that is late is called once and moves on from `now`, it does not
     * catch up the missed rounds.
     */
    Id every(std::chrono::milliseconds period, std::function<void()> function,
             Clock::time_point now = Clock::now());

    //! Remove the timer; false if it was already called or cancelled
    bool cancel(Id id);

    //! Milliseconds to the next timer, -1 for none (the `poll()` argument)
    int timeout_ms(Clock::time_point now = Clock::now()) const;

    //! Call the timers that are due, the earliest first; returns how many
    int run_due(Clock::time_point now = Clock::now());

    bool empty() const { return items.empty(); }

  private:
    struct Item {
        Clock::time_point at;
        std::chrono::milliseconds period; //!< zero for a timer that fires once
        std::function<void()> function;
    };

    Id next_id = 1;
    std::map<Id, Item> items;
};

} // namespace tiny
