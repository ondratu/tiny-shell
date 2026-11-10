#include "wm/idle_time.h"

#include <X11/extensions/sync.h>

#include <cstdint>
#include <cstring>

namespace tiny::wm {

IdleTime::IdleTime(::Display* display_arg): display(display_arg)
{
    int major = 0;
    int minor = 0;
    if (!XSyncInitialize(display, &major, &minor)) {
        return;
    }
    int count = 0;
    XSyncSystemCounter* list = XSyncListSystemCounters(display, &count);
    if (list == nullptr) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (list[i].name != nullptr &&
            std::strcmp(list[i].name, "IDLETIME") == 0) {
            counter = list[i].counter;
        }
    }
    XSyncFreeSystemCounterList(list);
}

std::optional<std::chrono::milliseconds> IdleTime::idle() const
{
    if (counter == None) {
        return std::nullopt;
    }
    XSyncValue value;
    if (!XSyncQueryCounter(display, counter, &value)) {
        return std::nullopt;
    }
    const std::int64_t ms =
            (static_cast<std::int64_t>(XSyncValueHigh32(value)) << 32) |
            XSyncValueLow32(value);
    return std::chrono::milliseconds(ms);
}

} // namespace tiny::wm
