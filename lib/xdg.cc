#include "xdg.h"

#include <algorithm>

namespace tiny {

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

std::vector<std::string> xdg_data_dirs(const char* data_home,
                                       const char* data_dirs, const char* home)
{
    std::vector<std::string> dirs;
    const auto add = [&](std::string_view dir) {
        const std::string clean = absolute_dir(dir);
        if (!clean.empty() &&
            std::find(dirs.begin(), dirs.end(), clean) == dirs.end()) {
            dirs.push_back(clean);
        }
    };

    if (data_home != nullptr && !absolute_dir(data_home).empty()) {
        add(data_home);
    } else if (home != nullptr && *home != '\0') {
        add(std::string(home) + "/.local/share");
    }

    const std::string_view system =
            (data_dirs != nullptr && *data_dirs != '\0')
                    ? std::string_view(data_dirs)
                    : std::string_view("/usr/local/share:/usr/share");
    for (const auto& dir : split_path_list(system)) {
        add(dir);
    }
    return dirs;
}

} // namespace tiny
