#include "application_index.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <set>

namespace tiny {

namespace fs = std::filesystem;

namespace {

// a loop of symbolic links must end
constexpr size_t MAX_DEPTH = 8;

std::vector<std::string> split_paths(std::string_view paths)
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

bool is_executable_file(const std::string& file)
{
    std::error_code error;
    return fs::is_regular_file(file, error) && access(file.c_str(), X_OK) == 0;
}

std::string lower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

// "/usr/share/" -> "/usr/share"; a relative path is nothing
std::string absolute_dir(std::string dir)
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

struct Scanner {
    std::vector<Application>& apps;
    std::string_view locale;
    std::string_view path;
    std::set<std::string> seen;

    struct Dir {
        fs::path path;
        std::string prefix; //!< id of the subdirectory: "kde-" for "kde/"
        std::vector<fs::path> parents; //!< real paths of the directories above
    };

    void scan(const fs::path& top)
    {
        std::error_code error;
        std::vector<Dir> todo{{top, "", {fs::canonical(top, error)}}};
        while (!todo.empty()) {
            const Dir dir = std::move(todo.back());
            todo.pop_back();
            std::vector<Dir> children = read(dir);
            // the first subdirectory is the next one
            std::move(children.rbegin(), children.rend(),
                      std::back_inserter(todo));
        }
    }

    // Add the files of the directory and return the subdirectories.
    std::vector<Dir> read(const Dir& dir)
    {
        std::error_code error;
        std::vector<fs::path> files;
        fs::directory_iterator it(
                dir.path, fs::directory_options::skip_permission_denied, error);
        for (const fs::directory_iterator end; !error && it != end;
             it.increment(error)) {
            files.push_back(it->path());
        }
        // the same result on every file system when ids collide
        std::sort(files.begin(), files.end());

        std::vector<Dir> children;
        for (const auto& file : files) {
            const auto status = fs::status(file, error); // follows links
            if (fs::is_directory(status)) {
                // a loop of symbolic links must end
                const fs::path real = fs::canonical(file, error);
                if (dir.parents.size() < MAX_DEPTH && !error &&
                    std::find(dir.parents.begin(), dir.parents.end(), real) ==
                            dir.parents.end()) {
                    Dir child{file, dir.prefix + file.filename().string() + "-",
                              dir.parents};
                    child.parents.push_back(real);
                    children.push_back(std::move(child));
                }
            } else if (fs::is_regular_file(status) &&
                       file.extension() == ".desktop") {
                add(file, dir.prefix + file.filename().string());
            }
        }
        return children;
    }

    void add(const fs::path& file, const std::string& id)
    {
        if (seen.count(id) != 0) {
            return;
        }
        auto entry = read_desktop_entry(file.string(), locale);
        if (!entry) {
            return; // a broken file doesn't hide a good one
        }
        seen.insert(id);
        if (!entry->visible()) {
            return;
        }
        if (!entry->try_exec.empty() &&
            !executable_exists(entry->try_exec, path)) {
            return;
        }
        apps.push_back({id, file.string(), std::move(*entry)});
    }
};

} // namespace

std::vector<std::string>
application_dirs(const char* data_home, const char* data_dirs, const char* home)
{
    std::vector<std::string> dirs;
    const auto add = [&](const std::string& dir) {
        const std::string clean = absolute_dir(dir);
        if (!clean.empty()) {
            const std::string applications =
                    (clean == "/" ? "" : clean) + "/applications";
            if (std::find(dirs.begin(), dirs.end(), applications) ==
                dirs.end()) {
                dirs.push_back(applications);
            }
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
    for (const auto& dir : split_paths(system)) {
        add(dir);
    }
    return dirs;
}

bool executable_exists(std::string_view program, std::string_view path)
{
    if (program.empty()) {
        return false;
    }
    if (program.find('/') != std::string_view::npos) {
        return is_executable_file(std::string(program));
    }
    const auto dirs = split_paths(path);
    return std::any_of(dirs.begin(), dirs.end(), [&](const std::string& dir) {
        return is_executable_file(dir + "/" + std::string(program));
    });
}

void ApplicationIndex::scan(const std::vector<std::string>& dirs,
                            std::string_view locale)
{
    const char* path = std::getenv("PATH");
    scan(dirs, locale, path != nullptr ? path : "");
}

void ApplicationIndex::scan(const std::vector<std::string>& dirs,
                            std::string_view locale, std::string_view path)
{
    apps.clear();
    Scanner scanner{apps, locale, path, {}};
    for (const auto& dir : dirs) {
        scanner.scan(dir);
    }

    std::stable_sort(apps.begin(), apps.end(),
                     [](const Application& a, const Application& b) {
                         const int order =
                                 std::strcoll(lower(a.entry.name).c_str(),
                                              lower(b.entry.name).c_str());
                         return order != 0 ? order < 0 : a.id < b.id;
                     });
}

} // namespace tiny
