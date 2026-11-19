#include "plugin/plugin_info.h"

#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#include "ini.h"
#include "text.h"
#include "xdg/dirs.h"

namespace tiny::plugin {

namespace fs = std::filesystem;

namespace {

constexpr size_t max_metadata = 64 * 1024;

std::optional<std::string> read_small_file(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::string text(max_metadata, '\0');
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(in.gcount()));
    return text;
}

bool plain_text(const std::string& text)
{
    return std::none_of(text.begin(), text.end(), [](char c) {
        return static_cast<unsigned char>(c) < 0x20 || c == 0x7f;
    });
}

bool valid_setting_key(const std::string& key)
{
    return !key.empty() && key != "enabled" && key.size() <= 64 &&
           std::all_of(key.begin(), key.end(), [](char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                      c == '_';
           });
}

std::optional<double> parse_number(const std::string& text)
{
    const std::string clean(trim(text));
    if (clean.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    const double value = strtod(clean.c_str(), &end);
    if (errno != 0 || end == clean.c_str() || *end != '\0' ||
        value != value) { // NaN
        return std::nullopt;
    }
    return value;
}

//! The setting `key` from its section; the reason is set when it is wrong
std::optional<SettingSpec> read_setting(const Ini& ini, const std::string& key,
                                        std::string& why)
{
    const std::string section = "setting." + key;
    const auto get = [&](const char* name) {
        return ini.get(section, name).value_or("");
    };
    SettingSpec spec;
    spec.key = key;
    const std::string type = get("type");
    if (type == "bool") {
        spec.type = SettingType::Toggle;
    } else if (type == "choice") {
        spec.type = SettingType::Choice;
    } else if (type == "text") {
        spec.type = SettingType::Text;
    } else if (type == "number") {
        spec.type = SettingType::Number;
    } else {
        why = "the type of the setting " + key +
              " must be bool, choice, text or number";
        return std::nullopt;
    }
    spec.label = get("label").empty() ? key : get("label");
    spec.note = get("note");
    spec.fallback = get("default");
    if (spec.type == SettingType::Toggle && spec.fallback.empty()) {
        spec.fallback = "no";
    }
    if (!plain_text(spec.label) || !plain_text(spec.note) ||
        !plain_text(spec.fallback)) {
        why = "the setting " + key + " has a control character";
        return std::nullopt;
    }
    if (spec.type == SettingType::Choice) {
        for (const std::string& item : split(get("choices"), ';', true)) {
            const size_t equal = item.find('=');
            SettingChoice choice;
            choice.value = std::string(trim(item.substr(0, equal)));
            choice.label = equal == std::string::npos
                                   ? choice.value
                                   : std::string(trim(item.substr(equal + 1)));
            if (choice.value.empty() || !plain_text(item)) {
                why = "the choices of the setting " + key + " are wrong";
                return std::nullopt;
            }
            spec.choices.push_back(std::move(choice));
        }
        if (spec.choices.empty()) {
            why = "the setting " + key + " has no choices";
            return std::nullopt;
        }
        if (spec.fallback.empty()) {
            spec.fallback = spec.choices.front().value;
        }
    }
    if (spec.type == SettingType::Number) {
        spec.min = parse_number(get("min"));
        spec.max = parse_number(get("max"));
        if (spec.min && spec.max && *spec.min > *spec.max) {
            why = "the range of the setting " + key + " is empty";
            return std::nullopt;
        }
        if (spec.fallback.empty()) {
            spec.fallback = number_text(spec.min.value_or(0));
        }
    }
    if (!valid_setting_value(spec, spec.fallback)) {
        why = "the default of the setting " + key + " is not a valid value";
        return std::nullopt;
    }
    return spec;
}

} // namespace

const SettingSpec* PluginInfo::find_setting(const std::string& key) const
{
    for (const SettingSpec& spec : settings) {
        if (spec.key == key) {
            return &spec;
        }
    }
    return nullptr;
}

PluginInfo parse_plugin_info(const std::string& id, const std::string& dir,
                             Origin origin, const std::string& text)
{
    PluginInfo info;
    info.id = id;
    info.name = id;
    info.dir = dir;
    info.origin = origin;

    Ini ini;
    std::vector<std::string> errors;
    ini.parse(text, &errors);
    const auto get = [&](const char* key) {
        return ini.get("plugin", key).value_or("");
    };
    const auto fail = [&](const std::string& why) {
        if (info.problem.empty()) {
            info.problem = why;
        }
    };
    if (!errors.empty()) {
        fail("plugin.ini, line " + errors.front());
    }
    if (!get("name").empty() && plain_text(get("name"))) {
        info.name = get("name");
    }
    info.description = plain_text(get("description")) ? get("description") : "";
    info.version = plain_text(get("version")) ? get("version") : "";
    info.author = plain_text(get("author")) ? get("author") : "";
    info.url = plain_text(get("url")) ? get("url") : "";
    info.default_enabled = get("enabled") == "yes";
    if (const auto api = ini.get("plugin", "api")) {
        const auto number = parse_number(*api);
        if (!number || *number < 1 || *number != static_cast<int>(*number)) {
            fail("api must be a whole number from 1");
        } else {
            info.api = static_cast<int>(*number);
        }
    }
    if (info.api > api_level) {
        fail("it needs the interface " + std::to_string(info.api) +
             ", this shell has " + std::to_string(api_level));
    }
    std::set<std::string> seen;
    for (const std::string& key : split(get("settings"), ';', true)) {
        std::string why;
        if (!valid_setting_key(key)) {
            fail("wrong name of a setting: " + key);
        } else if (!seen.insert(key).second) {
            fail("the setting " + key + " is listed twice");
        } else if (auto spec = read_setting(ini, key, why)) {
            info.settings.push_back(std::move(*spec));
        } else {
            fail(why);
        }
    }
    if (!fs::exists(fs::path(dir) / "plugin.lua")) {
        fail("plugin.lua is missing");
    }
    return info;
}

PluginDirs plugin_dirs(const char* data_home, const char* data_dirs,
                       const char* home)
{
    const std::string suffix = "/tiny-shell/plugins";
    PluginDirs dirs;
    const std::vector<std::string> system =
            xdg::data_dir_list(nullptr, data_dirs, nullptr);
    const std::vector<std::string> all =
            xdg::data_dir_list(data_home, data_dirs, home);
    if (all.size() > system.size()) {
        dirs.user = all.front() + suffix;
    }
    for (const std::string& dir : system) {
        dirs.system.push_back(dir + suffix);
    }
    return dirs;
}

namespace {

//! The plugins in a directory, the ones with a wrong id are skipped
std::vector<PluginInfo> read_dir(const std::string& dir, Origin origin)
{
    std::vector<PluginInfo> found;
    std::error_code ignore;
    for (fs::directory_iterator it(dir, ignore), end; !ignore && it != end;
         it.increment(ignore)) {
        const std::string id = it->path().filename().string();
        if (!wm::valid_plugin_id(id) || !fs::is_directory(it->path(), ignore)) {
            continue;
        }
        const fs::path ini = it->path() / "plugin.ini";
        const bool has_ini = fs::exists(ini, ignore);
        if (!has_ini && !fs::exists(it->path() / "plugin.lua", ignore)) {
            continue; // not a plugin
        }
        PluginInfo info = parse_plugin_info(id, it->path().string(), origin,
                                            read_small_file(ini).value_or(""));
        if (!has_ini) {
            info.problem = "plugin.ini is missing";
        }
        found.push_back(std::move(info));
    }
    return found;
}

} // namespace

std::vector<PluginInfo> find_plugins(const PluginDirs& dirs)
{
    std::map<std::string, PluginInfo> by_id;
    const auto add = [&](std::vector<PluginInfo> list) {
        for (PluginInfo& info : list) {
            const auto it = by_id.find(info.id);
            if (it == by_id.end()) {
                by_id.emplace(info.id, std::move(info));
            } else if (it->second.origin == Origin::System &&
                       info.origin == Origin::User) {
                info.shadows = true;
                info.shadowed_version = it->second.version;
                it->second = std::move(info);
            }
        }
    };
    // of the system directories the first one that has the id wins; a plugin
    // of the user is added last and takes the place of a system one
    for (const std::string& dir : dirs.system) {
        add(read_dir(dir, Origin::System));
    }
    if (!dirs.user.empty()) {
        add(read_dir(dirs.user, Origin::User));
    }
    std::vector<PluginInfo> list;
    list.reserve(by_id.size());
    for (auto& entry : by_id) {
        list.push_back(std::move(entry.second));
    }
    return list;
}

int compare_versions(const std::string& a, const std::string& b)
{
    const auto parts = [](const std::string& text) {
        std::vector<unsigned long> numbers;
        for (const std::string& part : split(text, '.')) {
            numbers.push_back(strtoul(part.c_str(), nullptr, 10));
        }
        return numbers;
    };
    std::vector<unsigned long> left = parts(a);
    std::vector<unsigned long> right = parts(b);
    const size_t size = std::max(left.size(), right.size());
    left.resize(size, 0);
    right.resize(size, 0);
    if (left == right) {
        return 0;
    }
    return left < right ? -1 : 1;
}

bool plugin_enabled(const PluginInfo& info, const wm::Config& config)
{
    if (!info.problem.empty()) {
        return false;
    }
    const auto it = config.plugins.find(info.id);
    if (it != config.plugins.end() && it->second.enabled) {
        return *it->second.enabled;
    }
    return info.default_enabled;
}

bool valid_setting_value(const SettingSpec& spec, const std::string& text)
{
    if (!plain_text(text)) {
        return false;
    }
    switch (spec.type) {
        case SettingType::Toggle:
            return text == "yes" || text == "no";
        case SettingType::Choice:
            return std::any_of(spec.choices.begin(), spec.choices.end(),
                               [&](const SettingChoice& choice) {
                                   return choice.value == text;
                               });
        case SettingType::Number: {
            const auto value = parse_number(text);
            return value && (!spec.min || *value >= *spec.min) &&
                   (!spec.max || *value <= *spec.max);
        }
        case SettingType::Text:
            break;
    }
    return true;
}

std::string setting_value(const SettingSpec& spec, const wm::Config& config,
                          const std::string& plugin_id)
{
    const auto plugin = config.plugins.find(plugin_id);
    if (plugin != config.plugins.end()) {
        const auto value = plugin->second.values.find(spec.key);
        if (value != plugin->second.values.end() &&
            valid_setting_value(spec, value->second)) {
            return value->second;
        }
    }
    return spec.fallback;
}

} // namespace tiny::plugin
