#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tiny::xdg {

/// Split a list like `$PATH` at colons; empty elements are dropped.
std::vector<std::string> split_path_list(std::string_view paths);

/**
 * @short The XDG data directories in the order of priority.
 *
 * `$XDG_DATA_HOME` (or `$HOME/.local/share`) first, then `$XDG_DATA_DIRS`
 * (or `/usr/local/share:/usr/share`). Pass the values of the variables, null
 * or empty means unset. Relative paths are ignored (as the specification
 * says) and a directory is listed only once. No trailing slash, except "/".
 */
std::vector<std::string> data_dir_list(const char* data_home,
                                       const char* data_dirs, const char* home);

} // namespace tiny::xdg
