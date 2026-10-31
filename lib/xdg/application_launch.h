#pragma once

#include <sys/types.h>

#include <string>
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
 */
std::vector<std::string> expand_exec(const Application& app,
                                     const std::vector<std::string>& files);

/**
 * @short Start an application, in its `Path` directory.
 *
 * With `Terminal=true` the program runs in `terminal`, which is the program
 * and the arguments that are followed by the command (e.g. `{"xterm", "-e"}`).
 *
 * Returns the pid, or -1 with errno set as `spawn` does (EINVAL for an entry
 * with no usable Exec).
 */
pid_t launch(const Application& app, const std::vector<std::string>& files = {},
             const std::vector<std::string>& terminal = {"xterm", "-e"});

} // namespace tiny::xdg
