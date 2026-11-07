#pragma once

#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>

#include "xdg/application_index.h"

namespace tiny::xdg {

/**
 * @short The arguments of the program that starts an application.
 *
 * The Exec value is split (see `split_exec`) and the field codes are
 * expanded: `%f` and `%u` are the first of `files`, `%F` and `%U` all of
 * them, `%i` is `--icon` and the Icon, `%c` the Name, `%k` the path of the
 * .desktop file and `%%` a percent sign. A code that is not known (also the
 * deprecated ones) and a code of a file without a file are dropped.
 * A non-empty `exec` is used instead of the Exec of the application (the
 * command of one of its actions).
 */
std::vector<std::string> expand_exec(const Application& app,
                                     const std::vector<std::string>& files,
                                     std::string_view exec = {});

/**
 * @short Start an application, in its `Path` directory.
 *
 * With `Terminal=true` the program runs in `terminal`, which is the program
 * and the arguments that are followed by the command (e.g. `{"xterm", "-e"}`).
 *
 *  * `startup_id` is given to the program in `DESKTOP_STARTUP_ID` (the startup
 * notification); the variable is removed when it is empty, so the program does
 * not take the one of the shell for its own.
 *
 * Returns the pid, or -1 with errno set as `spawn` does (EINVAL for an entry
 * with no usable Exec).
 */
pid_t launch(const Application& app, const std::vector<std::string>& files = {},
             const std::vector<std::string>& terminal = {"xterm", "-e"},
             std::string_view exec = {}, const std::string& startup_id = {});

} // namespace tiny::xdg
