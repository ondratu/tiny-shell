#include "application_index.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <numeric>
#include <set>

#include "xdg.h"

namespace tiny {

namespace fs = std::filesystem;

namespace {

// a loop of symbolic links must end
constexpr size_t MAX_DEPTH = 8;

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
    std::vector<std::string> dirs = xdg_data_dirs(data_home, data_dirs, home);
    for (auto& dir : dirs) {
        dir += dir == "/" ? "applications" : "/applications";
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
    const auto dirs = split_path_list(path);
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

    // the sort key is made once, not in every comparison
    std::vector<std::string> keys;
    keys.reserve(apps.size());
    for (const auto& app : apps) {
        keys.push_back(lower(app.entry.name));
    }
    std::vector<size_t> order(apps.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const int cmp = std::strcoll(keys[a].c_str(), keys[b].c_str());
        return cmp != 0 ? cmp < 0 : apps[a].id < apps[b].id;
    });

    std::vector<Application> sorted;
    sorted.reserve(apps.size());
    for (const size_t i : order) {
        sorted.push_back(std::move(apps[i]));
    }
    apps = std::move(sorted);
}

} // namespace tiny
