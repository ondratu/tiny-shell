#include "wm/lock_guard.h"

#include <sys/wait.h>

namespace tiny::wm {

void LockGuard::start()
{
    pid = hooks.start();
    if (pid < 0) {
        pid = 0;
        hooks.report("The screen was not locked: tiny-lock can't be started");
    }
}

void LockGuard::lock()
{
    if (running()) {
        return;
    }
    restarts = 0;
    start();
}

void LockGuard::check()
{
    int status = 0;
    if (!running() || !hooks.ended(pid, &status)) {
        return;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) { // unlocked
        pid = 0;
        return;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == cant_lock_status) {
        pid = 0;
        hooks.report("The screen was not locked, see the messages of "
                     "tiny-lock (tiny-lock --probe says if the password can be "
                     "checked)");
        return;
    }
    // a crash or a kill: the screen must not stay open
    if (restarts >= max_restarts) {
        pid = 0;
        hooks.report("tiny-lock keeps ending without an unlock; it is not "
                     "started again");
        return;
    }
    ++restarts;
    hooks.report("tiny-lock ended without an unlock, it is started again");
    start();
}

} // namespace tiny::wm
