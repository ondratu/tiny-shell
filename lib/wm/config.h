#pragma once

#include <string>
#include <vector>

namespace tiny::wm {

/**
 * @short The settings of tiny-shell, from the configuration files.
 *
 * The members have the defaults that are used when no file sets them.
 *
 * ```
 * [theme]
 * scale = 1.5
 *
 * [commands]
 * reboot = sudo reboot
 * poweroff = doas poweroff
 * terminal = xterm -e
 * ```
 */
struct Config {
    double scale = 1;                                //!< [theme] scale, (0, 16]
    std::string reboot_command = "systemctl reboot"; //!< [commands]
    std::string poweroff_command = "systemctl poweroff"; //!< [commands]
    std::vector<std::string> terminal = {"xterm", "-e"}; //!< [commands]
};

/**
 * @short The files to read, the one with the lowest priority first.
 *
 * `tiny-shell.ini` in every directory of the XDG configuration directories
 * (`$XDG_CONFIG_DIRS`, `/etc/xdg` by default), then the one of the user
 * (`$XDG_CONFIG_HOME`, `~/.config` by default). Pass the values of the
 * variables, null or empty means unset.
 */
std::vector<std::string> config_files(const char* config_home,
                                      const char* config_dirs,
                                      const char* home);

//! The text of a configuration file and its name for the messages
struct ConfigSource {
    std::string name;
    std::string text;
};

/**
 * @short Settings from the texts of the files, layered in the order given
 * (a later one wins).
 *
 * A line or a value that is wrong is described in `problems` (a line with the
 * name of its source) and the default is kept, or the value of a source with
 * a lower priority. Keys that are not known are ignored, so a file can be
 * shared with a newer version.
 */
Config parse_config(const std::vector<ConfigSource>& sources,
                    std::vector<std::string>* problems = nullptr);

/// Read the files (a file that is missing is not an error), see `parse_config`.
Config load_config(const std::vector<std::string>& files,
                   std::vector<std::string>* problems = nullptr);

} // namespace tiny::wm
