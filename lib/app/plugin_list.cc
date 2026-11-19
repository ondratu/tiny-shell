#include "app/plugin_list.h"

#include <filesystem>

namespace tiny::app {

std::vector<SettingsForm::PluginRow>
plugin_rows(const std::vector<plugin::PluginInfo>& found,
            const wm::Config& config)
{
    std::vector<SettingsForm::PluginRow> rows;
    rows.reserve(found.size());
    for (const plugin::PluginInfo& info : found) {
        SettingsForm::PluginRow row;
        row.id = info.id;
        row.name = info.name;
        row.description = info.description;
        row.version = info.version;
        row.user = info.origin == plugin::Origin::User;
        row.origin = row.user ? "User" : "System";
        if (info.shadows) {
            row.origin += info.shadowed_version.empty()
                                  ? ", replaces the system one"
                                  : ", replaces " + info.shadowed_version;
        }
        row.reason = info.problem;
        row.enabled = plugin_enabled(info, config);
        row.specs = info.settings;
        for (const plugin::SettingSpec& spec : info.settings) {
            row.values.push_back(plugin::setting_value(spec, config, info.id));
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<wm::IniValue>
plugin_changes(const std::vector<SettingsForm::PluginRow>& before,
               const std::vector<SettingsForm::PluginRow>& after)
{
    std::vector<wm::IniValue> changes;
    for (size_t i = 0; i < before.size() && i < after.size(); ++i) {
        const SettingsForm::PluginRow& was = before[i];
        const SettingsForm::PluginRow& now = after[i];
        if (was.id != now.id || now.remove) {
            continue;
        }
        const std::string section = "plugin." + now.id;
        if (now.enabled != was.enabled) {
            changes.push_back({section, "enabled", now.enabled ? "yes" : "no"});
        }
        for (size_t s = 0; s < now.specs.size() && s < now.values.size() &&
                           s < was.values.size();
             ++s) {
            if (now.values[s] != was.values[s]) {
                changes.push_back({section, now.specs[s].key, now.values[s]});
            }
        }
    }
    return changes;
}

bool remove_plugin_directory(const std::string& user_dir, const std::string& id,
                             std::string* error)
{
    namespace fs = std::filesystem;
    const auto fail = [&](const std::string& why) {
        if (error != nullptr) {
            *error = why;
        }
        return false;
    };
    if (user_dir.empty() || !wm::valid_plugin_id(id)) {
        return fail("wrong plugin " + id);
    }
    const fs::path path = fs::path(user_dir) / id;
    std::error_code code;
    const fs::file_status status = fs::symlink_status(path, code);
    if (!fs::exists(status)) {
        return true;
    }
    if (fs::is_symlink(status)) {
        fs::remove(path, code); // the link, never what it points to
    } else if (fs::is_directory(status)) {
        fs::remove_all(path, code);
    } else {
        return fail(path.string() + " is not a directory");
    }
    if (code) {
        return fail("can't remove " + path.string() + ": " + code.message());
    }
    return true;
}

} // namespace tiny::app
