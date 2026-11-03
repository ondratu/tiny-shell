#include "wm/config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "ini.h"
#include "xdg/application_launch.h"
#include "xdg/dirs.h"

namespace tiny::wm {

namespace {

constexpr double max_scale = 16;

} // namespace

std::vector<std::string> config_files(const char* config_home,
                                      const char* config_dirs, const char* home)
{
    std::vector<std::string> files;
    const auto dirs = xdg::config_dir_list(config_home, config_dirs, home);
    // the list starts with the user, the files are read from the other end
    for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) {
        files.push_back(*it + (*it == "/" ? "" : "/") + "tiny-shell/config");
    }
    return files;
}

namespace {

//! Set what `ini` has on `config`; a wrong value is told and left as it was
void apply(const Ini& ini, const std::string& name, Config& config,
           std::vector<std::string>* problems)
{
    const auto problem = [&](const char* what) {
        if (problems) {
            problems->push_back(name + ": " + what);
        }
    };

    if (const auto value = ini.get("theme", "scale")) {
        char* end = nullptr;
        const double scale = strtod(value->c_str(), &end);
        if (end == value->c_str() || *end != '\0' || !(scale > 0) ||
            scale > max_scale) {
            problem("[theme] scale must be a number above 0 and up to 16");
        } else {
            config.scale = scale;
        }
    }
    if (const auto value = ini.get("commands", "reboot"); value) {
        if (value->empty()) {
            problem("[commands] reboot must be a command");
        } else {
            config.reboot_command = *value;
        }
    }
    if (const auto value = ini.get("commands", "poweroff"); value) {
        if (value->empty()) {
            problem("[commands] poweroff must be a command");
        } else {
            config.poweroff_command = *value;
        }
    }
    if (const auto value = ini.get("commands", "terminal")) {
        auto terminal = xdg::split_exec(*value);
        if (terminal.empty()) {
            problem("[commands] terminal must be a program with arguments");
        } else {
            config.terminal = std::move(terminal);
        }
    }
}

} // namespace

Config parse_config(const std::vector<ConfigSource>& sources,
                    std::vector<std::string>* problems)
{
    Config config;
    for (const ConfigSource& source : sources) {
        Ini ini;
        std::vector<std::string> lines;
        ini.parse(source.text, &lines);
        if (problems) {
            for (const std::string& line : lines) {
                problems->push_back(source.name + ":" + line);
            }
        }
        apply(ini, source.name, config, problems);
    }
    return config;
}

Config load_config(const std::vector<std::string>& files,
                   std::vector<std::string>* problems)
{
    std::vector<ConfigSource> sources;
    for (const std::string& file : files) {
        const std::ifstream in(file);
        if (!in) {
            continue;
        }
        std::ostringstream text;
        text << in.rdbuf();
        sources.push_back({file, text.str()});
    }
    return parse_config(sources, problems);
}

} // namespace tiny::wm
