#pragma once

#include <optional>
#include <string>
#include <vector>

#include "wm/config.h"

namespace tiny::plugin {

//! The version of the interface that the module `tiny` gives to the plugins
//! (`api` in `plugin.ini` says what a plugin needs)
constexpr int api_level = 1;

//! The kinds of a setting of a plugin; the dialog of the settings is made
//! from them
enum class SettingType {
    Toggle, //!< yes or no (`bool` in plugin.ini; Xlib has a macro Bool)
    Choice, //!< one of a few values
    Text,   //!< any one line
    Number  //!< a number, perhaps in a range
};

//! A value of a setting of the type Choice, and its name for the dialog
struct SettingChoice {
    std::string value;
    std::string label;
};

//! One setting of a plugin, from `[setting.<key>]` of its `plugin.ini`
struct SettingSpec {
    std::string key;
    SettingType type = SettingType::Text;
    std::string label;    //!< the name in the dialog (the key when not given)
    std::string note;     //!< one line of help, may be empty
    std::string fallback; //!< the value when the user has not set one
    std::vector<SettingChoice> choices; //!< for the type Choice
    std::optional<double> min;          //!< for the type Number
    std::optional<double> max;
};

//! Where a plugin was found
enum class Origin { System, User };

/**
 * @short A plugin that is installed: what its `plugin.ini` says and where it
 * is.
 *
 * ```
 * [plugin]
 * name = Weather
 * description = The weather and the forecast for three days
 * version = 1.2
 * author = A Name
 * url = https://example.org/plugin
 * api = 1
 * enabled = no
 * settings = location;units
 *
 * [setting.location]
 * type = text
 * label = Location
 * note = The name of a town
 * default = Prague
 *
 * [setting.units]
 * type = choice
 * label = Units
 * choices = metric=Celsius;imperial=Fahrenheit
 * default = metric
 * ```
 *
 * `api` is the level of the interface that the plugin needs (1 when it is not
 * given); `enabled` says whether the plugin runs when the user has not
 * chosen (no when it is not given). The types of a setting are bool, choice,
 * text and number (with `min` and `max`).
 */
struct PluginInfo {
    std::string id;   //!< the name of the directory
    std::string name; //!< the id when `plugin.ini` has no name
    std::string description;
    std::string version;
    std::string author;
    std::string url;
    std::string dir; //!< the directory of the plugin
    Origin origin = Origin::System;
    //! A plugin of the user that has the id of a system one (it replaces it)
    bool shadows = false;
    std::string shadowed_version; //!< the version of the one that is replaced
    bool default_enabled = false;
    int api = 1;
    std::vector<SettingSpec> settings;
    //! Why the plugin can't run (a wrong `plugin.ini`, a newer interface, no
    //! `plugin.lua`); empty when it can
    std::string problem;

    //! The setting called `key`, or null
    const SettingSpec* find_setting(const std::string& key) const;
};

/**
 * @short Read the `plugin.ini` of a plugin.
 *
 * `id` and `dir` are of the directory. A plugin whose file is wrong is still
 * returned, with `problem` set, so that the list of plugins can show why it
 * does not run. The settings that are wrong are left out.
 */
PluginInfo parse_plugin_info(const std::string& id, const std::string& dir,
                             Origin origin, const std::string& text);

//! The directories with plugins: the one of the user and the system ones
//! (`tiny-shell/plugins` of the XDG data directories)
struct PluginDirs {
    std::string user; //!< empty when neither XDG_DATA_HOME nor HOME is known
    std::vector<std::string> system; //!< the order of the priority
};

//! The directories from the values of the variables (null or empty is unset)
PluginDirs plugin_dirs(const char* data_home, const char* data_dirs,
                       const char* home);

/**
 * @short The plugins that are installed, by their ids.
 *
 * A directory with `plugin.ini` or `plugin.lua` and an id that `valid_plugin_id`
 * accepts is a plugin; other directories and files are ignored. When the same
 * id is in more places the one of the user wins, then the system directory
 * that comes first, and the plugin of the user knows that it replaces another
 * one (`shadows`).
 */
std::vector<PluginInfo> find_plugins(const PluginDirs& dirs);

//! -1, 0 or 1 when the version `a` is older, the same or newer than `b`.
//! The numbers between dots are compared as numbers (1.10 is newer than 1.9),
//! a text after a number is ignored.
int compare_versions(const std::string& a, const std::string& b);

//! Whether the plugin runs: the choice of the user, else its own default; a
//! plugin that has a problem never runs
bool plugin_enabled(const PluginInfo& info, const wm::Config& config);

//! Whether the text is a good value of the setting (a choice is one of its
//! values, a number is in the range, a bool is yes or no, a text has no
//! control character)
bool valid_setting_value(const SettingSpec& spec, const std::string& text);

//! The value of a setting: the one in the configuration when it is valid,
//! else the default of the plugin
std::string setting_value(const SettingSpec& spec, const wm::Config& config,
                          const std::string& plugin_id);

} // namespace tiny::plugin
