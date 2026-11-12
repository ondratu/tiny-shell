#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "xdg/application_index.h"

namespace tiny::xdg {

/**
 * @short The applications that start with the session (XDG Autostart).
 *
 * `config_dirs` are the configuration directories in the order of priority
 * (see `config_dir_list`); the files are in their `autostart` subdirectories.
 * A file in an earlier directory hides the file with the same name in a later
 * one, also when it is `Hidden=true` (that is how a user turns off a file of
 * the system); a file that can't be read hides nothing.
 *
 * Which files are skipped: see doc/session.md#autostart. `desktops` is the
 * value of `$XDG_CURRENT_DESKTOP`, `path` is where `TryExec` is looked for.
 *
 * Sorted by the name of the file, so the order is the same on every system.
 */
std::vector<Application>
autostart_applications(const std::vector<std::string>& config_dirs,
                       std::string_view desktops, std::string_view path);

} // namespace tiny::xdg
