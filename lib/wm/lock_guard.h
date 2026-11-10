#pragma once

#include <sys/types.h>

#include <functional>
#include <string>

namespace tiny::wm {

/**
 * @short Starts the screen lock and watches it, so that a crash of the lock
 * does not unlock the screen.
 *
 * The lock is a program of its own (`tiny-lock`), so the screen stays locked
 * when the shell is restarted, but it is not when the lock itself dies. The
 * guard starts it again when it ends in any other way than by its exit status:
 *
 *  - 0: the screen was unlocked, the guard is done;
 *  - 1: the lock could not lock the screen (no way to check the password,
 *    another program holds the keyboard): it has said why, and trying again
 *    would not help;
 *  - anything else (a signal, a crash, another status): the lock is started
 *    again, up to `max_restarts` times in a row.
 */
class LockGuard {
  public:
    static constexpr int max_restarts = 5;
    static constexpr int cant_lock_status = 1;
    //! A lock that ran this many calls of `check()` (500 ms each: a minute)
    //! without ending was not crashing in a loop: the count starts again
    static constexpr int stable_checks = 120;

    struct Hooks {
        //! Start the lock; its pid, -1 if it can't be started
        std::function<pid_t()> start;
        //! Whether the child has ended and its wait status
        std::function<bool(pid_t, int*)> ended;
        //! Tell the user something went wrong
        std::function<void(const std::string&)> report;
    };

    explicit LockGuard(Hooks hooks_arg): hooks(std::move(hooks_arg)) {}

    //! Lock the screen; nothing happens when it is locked already
    void lock();

    //! Look whether the lock has ended; call it every now and then while
    //! `running()`
    void check();

    //! Whether a lock is running
    bool running() const { return pid > 0; }

  private:
    void start();

    Hooks hooks;
    pid_t pid = 0;
    int restarts = 0; //!< starts in a row after a crash
    int checks = 0;   //!< calls of `check()` since the last start
};

} // namespace tiny::wm
