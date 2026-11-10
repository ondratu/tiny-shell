#include "wm/idle_lock.h"

#include <algorithm>

namespace tiny::wm {

std::optional<IdleLock::Duration> IdleLock::check() const
{
    if (limit <= Duration::zero()) {
        return std::nullopt;
    }
    const std::optional<Duration> idle = hooks.idle();
    if (!idle) {
        return std::nullopt;
    }
    if (*idle >= limit) {
        if (!hooks.locked()) {
            hooks.lock();
        }
        // whoever unlocks has touched the keyboard, so there is nothing to
        // look at before a whole limit has passed again
        return limit;
    }
    return std::max(limit - *idle, min_wait);
}

} // namespace tiny::wm
