#include "xdg/autostart.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace tiny::xdg {

namespace {

//! Why a file that is not `Hidden` does not start; empty when it starts
const char* skip_reason(const DesktopEntry& entry, std::string_view desktops,
                        std::string_view path)
{
    if (entry.exec.empty()) {
        return "has no command";
    }
    if (!entry.autostart_enabled) {
        return "turned off in the file";
    }
    if (!entry.autostart_condition.empty()) {
        return "has a condition";
    }
    if (!entry.shown_in(desktops)) {
        return "not for this desktop";
    }
    if (!entry.try_exec.empty() && !executable_exists(entry.try_exec, path)) {
        return "not installed";
    }
    return "";
}

std::string trimmed(const std::string& line)
{
    const size_t begin = line.find_first_not_of(" \t\r");
    const size_t end = line.find_last_not_of(" \t\r");
    return begin == std::string::npos ? ""
                                      : line.substr(begin, end - begin + 1);
}

bool is_hidden_key(const std::string& line)
{
    const std::string text = trimmed(line);
    return text.compare(0, 6, "Hidden") == 0 &&
           trimmed(text.substr(6)).compare(0, 1, "=") == 0;
}

bool has_exec(const std::string& line)
{
    const std::string text = trimmed(line);
    return text.compare(0, 4, "Exec") == 0 &&
           trimmed(text.substr(4)).compare(0, 1, "=") == 0;
}

std::vector<std::string> read_lines(const fs::path& file)
{
    std::vector<std::string> lines;
    std::ifstream in(file);
    for (std::string line; std::getline(in, line);) {
        lines.push_back(line);
    }
    return lines;
}

bool write_lines(const fs::path& file, const std::vector<std::string>& lines)
{
    std::ostringstream text;
    for (const std::string& line : lines) {
        text << line << '\n';
    }
    const fs::path temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << text.str();
        if (!out.flush()) {
            return false;
        }
    }
    std::error_code error;
    fs::rename(temp, file, error);
    if (error) {
        fs::remove(temp, error);
    }
    return !error;
}

} // namespace

std::vector<AutostartItem>
autostart_items(const std::vector<std::string>& config_dirs,
                std::string_view desktops, std::string_view path)
{
    std::vector<AutostartItem> items;
    std::map<std::string, size_t> found; // the id to its place in `items`

    for (size_t index = 0; index < config_dirs.size(); ++index) {
        std::error_code error;
        fs::directory_iterator it(fs::path(config_dirs[index]) / "autostart",
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
            if (file.extension() != ".desktop" ||
                !fs::is_regular_file(file, error)) { // follows links
                continue;
            }
            auto entry = read_desktop_entry(file.string());
            if (!entry) {
                continue; // a broken file does not hide a good one
            }
            const auto seen = found.find(id);
            if (seen != found.end()) {
                // a file of an earlier directory counts; this one only tells
                // that the program is of the system and what it is called
                AutostartItem& item = items[seen->second];
                item.system = true;
                if (item.name.empty()) {
                    item.name = entry->name;
                }
                continue;
            }
            if (entry->type != "Application") {
                found.emplace(id, items.size()); // it hides the later ones
                items.push_back({});             // and is dropped below
                items.back().app.id = id;
                continue;
            }
            AutostartItem item;
            item.name = entry->name;
            item.system = index > 0;
            if (entry->hidden) {
                item.state = index == 0 ? AutostartItem::State::Off
                                        : AutostartItem::State::Skipped;
                item.reason = "turned off";
            } else if (const char* why = skip_reason(*entry, desktops, path);
                       *why != '\0') {
                item.state = AutostartItem::State::Skipped;
                item.reason = why;
            }
            item.app = {id, file.string(), std::move(*entry)};
            found.emplace(id, items.size());
            items.push_back(std::move(item));
        }
    }
    items.erase(std::remove_if(items.begin(), items.end(),
                               [](const AutostartItem& item) {
                                   return item.app.path.empty();
                               }),
                items.end());
    std::sort(items.begin(), items.end(),
              [](const AutostartItem& a, const AutostartItem& b) {
                  return a.app.id < b.app.id;
              });
    return items;
}

std::vector<Application>
autostart_applications(const std::vector<std::string>& config_dirs,
                       std::string_view desktops, std::string_view path)
{
    std::vector<Application> apps;
    for (AutostartItem& item : autostart_items(config_dirs, desktops, path)) {
        if (item.state == AutostartItem::State::On) {
            apps.push_back(std::move(item.app));
        }
    }
    return apps;
}

bool set_autostart_enabled(const std::string& user_config_dir,
                           const AutostartItem& item, bool enabled)
{
    const fs::path dir = fs::path(user_config_dir) / "autostart";
    const fs::path file = dir / item.app.id;
    std::error_code error;
    if (!fs::exists(file, error)) {
        // a program of the system: only turning it off needs a file
        fs::create_directories(dir, error);
        return enabled ? !error
                       : write_lines(file, {"[Desktop Entry]",
                                            "Type=Application", "Hidden=true"});
    }
    // the file of the user: `Hidden` is in the first group
    std::vector<std::string> lines;
    bool in_entry = false;
    bool has_command = false;
    for (std::string& line : read_lines(file)) {
        const std::string text = trimmed(line);
        if (!text.empty() && text.front() == '[') {
            in_entry = text == "[Desktop Entry]";
            lines.push_back(std::move(line));
            if (in_entry && !enabled) {
                lines.emplace_back("Hidden=true");
            }
        } else if (in_entry && is_hidden_key(line)) {
            continue;
        } else {
            has_command = has_command || (in_entry && has_exec(line));
            lines.push_back(std::move(line));
        }
    }
    if (enabled && !has_command && item.system) {
        return fs::remove(file, error) && !error; // what turned it off was all
    }
    return write_lines(file, lines);
}

} // namespace tiny::xdg
