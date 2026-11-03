#include <algorithm>

#include "timers.h"

namespace tiny {

Timers::Id Timers::after(std::chrono::milliseconds delay,
                         std::function<void()> function, Clock::time_point now)
{
    const Id id = next_id++;
    items[id] = {now + std::max(delay, std::chrono::milliseconds(0)),
                 std::chrono::milliseconds(0), std::move(function)};
    return id;
}

Timers::Id Timers::every(std::chrono::milliseconds period,
                         std::function<void()> function, Clock::time_point now)
{
    period = std::max(period, std::chrono::milliseconds(1));
    const Id id = next_id++;
    items[id] = {now + period, period, std::move(function)};
    return id;
}

bool Timers::cancel(Id id)
{
    return items.erase(id) > 0;
}

int Timers::timeout_ms(Clock::time_point now) const
{
    int rv = -1;
    for (const auto& it : items) {
        const auto left = std::chrono::ceil<std::chrono::milliseconds>(
                it.second.at - now);
        const int ms = static_cast<int>(
                std::clamp<long long>(left.count(), 0, 1 << 30));
        rv = rv < 0 ? ms : std::min(rv, ms);
    }
    return rv;
}

int Timers::run_due(Clock::time_point now)
{
    // A function may add or cancel timers: take the ids first and look each
    // one up again. A timer added by a function waits for the next round.
    std::multimap<Clock::time_point, Id> due;
    for (const auto& it : items) {
        if (it.second.at <= now) {
            due.emplace(it.second.at, it.first);
        }
    }
    int called = 0;
    for (const auto& entry : due) {
        const auto it = items.find(entry.second);
        if (it == items.end()) {
            continue; // cancelled by an earlier function
        }
        // move the function out: no copy of its captures on every tick
        auto function = std::move(it->second.function);
        const bool periodic = it->second.period.count() > 0;
        if (periodic) {
            it->second.at = now + it->second.period;
        } else {
            items.erase(it);
        }
        function();
        if (periodic) {
            // put it back, unless the function cancelled its own timer
            const auto again = items.find(entry.second);
            if (again != items.end()) {
                again->second.function = std::move(function);
            }
        }
        ++called;
    }
    return called;
}

} // namespace tiny
