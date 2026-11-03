#include "xdg/dirs.h"

#include <algorithm>

namespace tiny::xdg {

namespace {

// "/usr/share/" -> "/usr/share"; a relative path is nothing
std::string absolute_dir(std::string_view dir)
{
    if (dir.empty() || dir[0] != '/') {
        return {};
    }
    std::string out;
    for (const char c : dir) {
        if (c != '/' || out.empty() || out.back() != '/') {
            out += c;
        }
    }
    if (out.size() > 1 && out.back() == '/') {
        out.pop_back();
    }
    return out;
}

} // namespace

std::vector<std::string> split_path_list(std::string_view paths)
{
    std::vector<std::string> out;
    while (!paths.empty()) {
        const auto colon = paths.find(':');
        const auto part = paths.substr(0, colon);
        if (!part.empty()) {
            out.emplace_back(part);
        }
        paths.remove_prefix(colon == std::string_view::npos ? paths.size()
                                                            : colon + 1);
    }
    return out;
}

namespace {

// The user directory first (`user` when it is an absolute path, otherwise
// `$HOME/<fallback>`), then the directories of `system` without repeats
std::vector<std::string> dir_list(const char* user, const char* home,
                                  const char* fallback, const char* system,
                                  const char* default_system)
{
    std::vector<std::string> dirs;
    const auto add = [&](std::string_view dir) {
        const std::string clean = absolute_dir(dir);
        if (!clean.empty() &&
            std::find(dirs.begin(), dirs.end(), clean) == dirs.end()) {
            dirs.push_back(clean);
        }
    };

    if (user != nullptr && !absolute_dir(user).empty()) {
        add(user);
    } else if (home != nullptr && *home != '\0') {
        add(std::string(home) + fallback);
    }

    const std::string_view list = (system != nullptr && *system != '\0')
                                          ? std::string_view(system)
                                          : std::string_view(default_system);
    for (const auto& dir : split_path_list(list)) {
        add(dir);
    }
    return dirs;
}

} // namespace

std::vector<std::string> data_dir_list(const char* data_home,
                                       const char* data_dirs, const char* home)
{
    return dir_list(data_home, home, "/.local/share", data_dirs,
                    "/usr/local/share:/usr/share");
}

std::vector<std::string> config_dir_list(const char* config_home,
                                         const char* config_dirs,
                                         const char* home)
{
    return dir_list(config_home, home, "/.config", config_dirs, "/etc/xdg");
}

} // namespace tiny::xdg
