#include "wm/session_end.h"

#include <algorithm>

namespace tiny::wm {

bool SessionEnd::begin(SessionKind what, Clock::time_point now)
{
    if (state != Phase::Idle) {
        return false;
    }
    state = Phase::Confirm;
    what_kind = what;
    deadline = now + confirm_timeout;
    return true;
}

void SessionEnd::proceed()
{
    if (state != Phase::Idle) {
        state = Phase::Closing;
    }
}

void SessionEnd::cancel()
{
    state = Phase::Idle;
}

int SessionEnd::seconds_left(Clock::time_point now) const
{
    if (state != Phase::Confirm || now >= deadline) {
        return 0;
    }
    using namespace std::chrono;
    return static_cast<int>(ceil<seconds>(deadline - now).count());
}

bool SessionEnd::timed_out(Clock::time_point now) const
{
    return state == Phase::Confirm && now >= deadline;
}

std::string session_title(SessionKind kind)
{
    switch (kind) {
        case SessionKind::Restart:
            return "Restart";
        case SessionKind::Shutdown:
            return "Shut Down";
        case SessionKind::Logout:
            break;
    }
    return "Log Out";
}

std::string session_proceed_label(SessionKind kind, bool anyway)
{
    return session_title(kind) + (anyway ? " Anyway" : "");
}

std::string session_question(SessionKind kind, int seconds)
{
    const std::string time = " automatically in " + std::to_string(seconds) +
                             (seconds == 1 ? " second." : " seconds.");
    switch (kind) {
        case SessionKind::Restart:
            return "The computer will restart" + time;
        case SessionKind::Shutdown:
            return "The computer will shut down" + time;
        case SessionKind::Logout:
            break;
    }
    return "You will be logged out" + time;
}

std::string session_waiting(const std::vector<std::string>& names)
{
    constexpr size_t listed = 3;
    if (names.empty()) {
        return "Waiting for the applications to close.";
    }
    std::string text = "Waiting for ";
    const size_t shown = std::min(names.size(), listed);
    for (size_t i = 0; i < shown; ++i) {
        if (i > 0) {
            // "A, B and C", and the others are counted when there are more
            text += i + 1 == shown && names.size() == shown ? " and " : ", ";
        }
        text += names[i];
    }
    if (names.size() > shown) {
        const size_t more = names.size() - shown;
        text += " and " + std::to_string(more) + " more";
    }
    return text + " to close.";
}

} // namespace tiny::wm
