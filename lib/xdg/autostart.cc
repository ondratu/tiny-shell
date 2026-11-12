#include "xdg/autostart.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <system_error>

namespace fs = std::filesystem;

namespace tiny::xdg {

std::vector<Application>
autostart_applications(const std::vector<std::string>& config_dirs,
                       std::string_view desktops, std::string_view path)
{
    std::vector<Application> apps;
    std::set<std::string> seen;

    for (const std::string& dir : config_dirs) {
        std::error_code error;
        fs::directory_iterator it(fs::path(dir) / "autostart",
                                  fs::directory_options::skip_permission_denied,
                                  error);
        std::vector<fs::path> files;
        for (const fs::directory_iterator end; !error && it != end;
             it.increment(error)) {
            files.push_back(it->path());
        }
        std::sort(files.begin(), files.end());

        for (const fs::path& file : files) {
            const std::string id = file.filename().string();
            if (file.extension() != ".desktop" || seen.count(id) != 0 ||
                !fs::is_regular_file(file, error)) { // follows links
                continue;
            }
            auto entry = read_desktop_entry(file.string());
            if (!entry) {
                continue; // a broken file does not hide a good one
            }
            seen.insert(id);
            if (entry->type != "Application" || entry->exec.empty() ||
                entry->hidden || !entry->autostart_enabled ||
                !entry->shown_in(desktops) ||
                (!entry->try_exec.empty() &&
                 !executable_exists(entry->try_exec, path))) {
                continue;
            }
            apps.push_back({id, file.string(), std::move(*entry)});
        }
    }
    std::sort(apps.begin(), apps.end(),
              [](const Application& a, const Application& b) {
                  return a.id < b.id;
              });
    return apps;
}

} // namespace tiny::xdg
