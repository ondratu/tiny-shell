#pragma once

#include <string>
#include <vector>

#include "wm/system_info.h"

namespace tiny::app {

//! The name of the shell, the title of `tiny-about`
inline constexpr const char* about_title = "Tiny Shell";

//! The address of the project
inline constexpr const char* about_project =
        "https://github.com/ondratu/tiny-shell";

//! The license of the shell
inline constexpr const char* about_license = "BSD 2-Clause License";

/**
 * @short The lines under the name of the shell in the about window: what it
 * is, the version, the system and the kernel, the license and the address.
 *
 * Only the lines that have something to tell are there, so the system that
 * is not known is told ("System unknown") and the kernel is left out when it
 * is not known.
 */
std::vector<std::string> about_lines(const std::string& version,
                                     const wm::SystemInfo& info);

} // namespace tiny::app
