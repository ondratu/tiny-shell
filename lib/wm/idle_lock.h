#pragma once

#include <chrono>
#include <functional>
#include <optional>

namespace tiny::wm {

/**
 * @short Locks the screen after a time without input (`[display] off_after` plus `[lock] delay`).
 *
 * The class only decides. How long the user has been away and how to lock come
 * from hooks, so the logic runs in the tests without a display. `check()` tells
 * when to call it again: the time that is left to the limit, not a short
 * period, so a computer that nobody uses is woken once in the limit and not
 * every few seconds.
 */
class IdleLock {
  public:
    using Duration = std::chrono::milliseconds;

    struct Hooks {
        //! The time since the last input; empty when it can't be told
        std::function<std::optional<Duration>()> idle;
        //! Whether the screen is locked at the moment
        std::function<bool()> locked;
        //! Lock the screen
        std::function<void()> lock;
    };

    //! The shortest time between two calls of `check()`
    static constexpr Duration min_wait = std::chrono::seconds(1);

    explicit IdleLock(Hooks hooks_arg): hooks(std::move(hooks_arg)) {}

    //! The limit; zero is never (`check()` then asks for no more calls)
    void set_limit(Duration value) { limit = value; }

    Duration get_limit() const { return limit; }

    /**
     * @short Lock when the user has been away for the limit.
     *
     * Returns when to call it again, or an empty value when there is no need
     * (no limit, or the idle time is not known).
     */
    std::optional<Duration> check() const;

  private:
    Hooks hooks;
    Duration limit{0};
};

} // namespace tiny::wm
