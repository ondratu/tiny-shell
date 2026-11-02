#pragma once

#include <sys/types.h>

#include <string>

namespace tiny::wm {

//! Name of this computer, what a client writes to `WM_CLIENT_MACHINE`
std::string local_host_name();

/**
 * @short The process that a client may be killed by, 0 if there is none.
 *
 * `pid` is `_NET_WM_PID` and `machine` is `WM_CLIENT_MACHINE` of the client.
 * Only a process that runs on `host`, that is not the init and not the window
 * manager itself can be signaled; the client may write anything there.
 */
pid_t killable_pid(unsigned long pid, const std::string& machine,
                   const std::string& host);

} // namespace tiny::wm
