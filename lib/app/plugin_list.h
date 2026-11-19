#pragma once

#include <string>
#include <vector>

#include "app/settings_form.h"
#include "plugin/plugin_info.h"
#include "wm/config.h"

namespace tiny::app {

/**
 * @short The rows of the list of the plugins of tiny-settings.
 *
 * A row is a plugin that is installed: whether it runs (the choice of the
 * user or its own default), where it comes from ("System", "User", "User,
 * replaces 1.0" for a plugin of the user that has the id of a system one) and
 * the value of each of its settings (the one of the configuration, or the
 * default). A plugin that has a problem can't be switched on; the problem is
 * its `reason`.
 */
std::vector<SettingsForm::PluginRow>
plugin_rows(const std::vector<plugin::PluginInfo>& found,
            const wm::Config& config);

/**
 * @short What to write to the file of the user so that the configuration
 * gets from `before` to `after` (the rows of `plugin_rows()` and the same
 * ones after the user changed them).
 *
 * Only what differs: `[plugin.<id>] enabled` when the plugin was switched and
 * the keys of the settings that have another value. A plugin that is to be
 * removed is not written.
 */
std::vector<wm::IniValue>
plugin_changes(const std::vector<SettingsForm::PluginRow>& before,
               const std::vector<SettingsForm::PluginRow>& after);

/**
 * @short Delete the directory `<user_dir>/<id>` of a plugin of the user.
 *
 * Only a directory that is in `user_dir` is deleted (the id must be a good
 * one, so there is no way out of it); a symbolic link is removed, not what it
 * points to. A plugin that is not there is not an error. Returns false and
 * tells why in `error` when the directory can't be deleted.
 */
bool remove_plugin_directory(const std::string& user_dir, const std::string& id,
                             std::string* error = nullptr);

} // namespace tiny::app
