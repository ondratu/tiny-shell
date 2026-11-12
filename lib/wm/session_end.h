#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace tiny::wm {

//! How the session ends
enum class SessionKind {
    Logout,   //!< the shell ends, and with it the session
    Restart,  //!< the command of `[commands] reboot` runs
    Shutdown, //!< the command of `[commands] poweroff` runs
};

/**
 * @short The end of a session, without X: what the dialog says and how long it
 * waits.
 *
 * The phases are described in doc/session.md#ending-the-session:
 * `Phase::Confirm` (ends by itself after `confirm_timeout`) and
 * `Phase::Closing` (waits as long as it takes, the user decides).
 */
class SessionEnd {
  public:
    using Clock = std::chrono::steady_clock;

    enum class Phase { Idle, Confirm, Closing };

    //! How long the question stays before the session ends by itself
    static constexpr std::chrono::seconds confirm_timeout{60};

    //! Ask the user; false (nothing changes) when the session is ending
    bool begin(SessionKind what, Clock::time_point now = Clock::now());

    //! The user agreed, or the time is over: the applications are asked to close
    void proceed();

    //! The user changed their mind
    void cancel();

    Phase phase() const { return state; }
    SessionKind kind() const { return what_kind; }
    bool idle() const { return state == Phase::Idle; }

    //! Seconds of the question that are left, rounded up, 0 when over
    int seconds_left(Clock::time_point now = Clock::now()) const;

    //! Whether the time of the question is over
    bool timed_out(Clock::time_point now = Clock::now()) const;

  private:
    Phase state = Phase::Idle;
    SessionKind what_kind = SessionKind::Logout;
    Clock::time_point deadline;
};

//! "Log Out", "Restart" or "Shut Down": the title of the dialog
std::string session_title(SessionKind kind);

//! The label of the button that agrees; "Log Out Anyway" for `anyway`, when the
//! applications that do not close are not waited for any more
std::string session_proceed_label(SessionKind kind, bool anyway);

//! "You will be logged out automatically in 60 seconds."
std::string session_question(SessionKind kind, int seconds);

//! "Waiting for Firefox and Terminal to close.", at most three names and the
//! number of the others
std::string session_waiting(const std::vector<std::string>& names);

} // namespace tiny::wm
