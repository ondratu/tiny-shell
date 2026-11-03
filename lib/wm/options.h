#pragma once

#include <string>
#include <vector>

#include "wm/config.h"

namespace tiny::wm {

//! What the command line asks tiny-shell to do
struct Options {
    enum class Action { Run, Help, Version, Info, Error };

    Action action = Action::Run;
    bool replace = false; //!< take over from the running window manager
    std::string error;    //!< what is wrong, for Action::Error
};

/**
 * @short Read the command line; `args` are the arguments without the name of
 * the program.
 *
 * `--help` (`-h`), `--version` (`-V`) and `--info` win over the rest, so they
 * work together with any other valid option.
 */
Options parse_options(const std::vector<std::string>& args);

//! The version, from `git describe` at the time of the build
std::string version_text();

//! The usage with the list of the options
std::string usage_text(const std::string& program);

/**
 * @short What tiny-shell is and what it is going to use.
 *
 * `config` is the settings, `files` the configuration files that were read
 * and `app_dirs` the `applications` directories of the launcher.
 */
std::string info_text(const Config& config,
                      const std::vector<std::string>& files,
                      const std::vector<std::string>& app_dirs);

} // namespace tiny::wm
