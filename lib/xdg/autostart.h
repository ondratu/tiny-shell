#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "xdg/application_index.h"

namespace tiny::xdg {

//! A program of the autostart as the list in `tiny-settings` shows it
struct AutostartItem {
    //! Starts, turned off by the user (`Hidden=true` in a file of the user:
    //! the list can turn it on), or not started for a `reason` the list
    //! can't change
    enum class State { On, Off, Skipped };

    Application app; //!< the file that counts
    //! The Name, from the file or, when it has none, from the one of the system
    std::string name;
    //! A file of this name is in a directory of the system: the user can turn
    //! the program off but not remove it
    bool system = false;
    State state = State::On;
    std::string reason; //!< why a skipped program does not start
};

/**
 * @short The programs of the autostart, also the ones that do not start.
 *
 * The rules are the ones of `autostart_applications`, which is the part of the
 * list that is `State::On`. Sorted by the name of the file.
 */
std::vector<AutostartItem>
autostart_items(const std::vector<std::string>& config_dirs,
                std::string_view desktops, std::string_view path);

/**
 * @short Turn a program of the autostart on or off for the user.
 *
 * `user_config_dir` is the first of the configuration directories. A file
 * there gets (or loses) `Hidden=true`; a file of the system is turned off with
 * a file of the same name there that has nothing else, and turning it on
 * removes that file again. Returns false when a file can't be written.
 */
bool set_autostart_enabled(const std::string& user_config_dir,
                           const AutostartItem& item, bool enabled);

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
