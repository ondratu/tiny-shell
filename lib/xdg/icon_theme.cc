#include "xdg/icon_theme.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>

#include "xdg/dirs.h"

namespace tiny::xdg {

namespace fs = std::filesystem;

namespace {

// the order of preference at one size; a vector file is the last
constexpr std::string_view EXTENSIONS[] = {".png", ".xpm", ".svg"};

using Section = std::map<std::string, std::string>;

std::string_view trim(std::string_view text)
{
    const char* const blanks = " \t\r";
    const auto first = text.find_first_not_of(blanks);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(blanks) - first + 1);
}

// the groups of an index.theme (an ini file); the keys are not localized
std::map<std::string, Section> read_groups(const fs::path& file)
{
    std::map<std::string, Section> groups;
    std::ifstream in(file);
    Section* group = nullptr;
    for (std::string raw; std::getline(in, raw);) {
        const std::string_view line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line[0] == '[' && line.back() == ']') {
            group = &groups[std::string(line.substr(1, line.size() - 2))];
        } else if (const auto equal = line.find('=');
                   group != nullptr && equal != std::string_view::npos) {
            (*group)[std::string(trim(line.substr(0, equal)))] =
                    std::string(trim(line.substr(equal + 1)));
        }
    }
    return groups;
}

std::vector<std::string> split_commas(std::string_view list)
{
    std::vector<std::string> out;
    while (!list.empty()) {
        const auto comma = list.find(',');
        if (const auto item = trim(list.substr(0, comma)); !item.empty()) {
            out.emplace_back(item);
        }
        list.remove_prefix(comma == std::string_view::npos ? list.size()
                                                           : comma + 1);
    }
    return out;
}

int to_int(const Section& section, const char* key, int fallback)
{
    const auto it = section.find(key);
    int value = fallback;
    if (it != section.end()) {
        const char* const end = it->second.data() + it->second.size();
        if (std::from_chars(it->second.data(), end, value).ec != std::errc()) {
            return fallback;
        }
    }
    return value;
}

bool is_file(const std::string& file)
{
    std::error_code error;
    return fs::is_regular_file(file, error);
}

// "firefox.png" -> "firefox": some entries give the extension anyway
std::string_view without_extension(std::string_view name)
{
    for (const auto extension : EXTENSIONS) {
        if (name.size() > extension.size() &&
            name.substr(name.size() - extension.size()) == extension) {
            return name.substr(0, name.size() - extension.size());
        }
    }
    return name;
}

} // namespace

std::vector<std::string> icon_dirs(const char* data_home, const char* data_dirs,
                                   const char* home)
{
    std::vector<std::string> dirs;
    if (home != nullptr && *home == '/') {
        dirs.push_back(std::string(home) + "/.icons");
    }
    for (const auto& dir : data_dir_list(data_home, data_dirs, home)) {
        dirs.push_back(dir + (dir == "/" ? "icons" : "/icons"));
    }
    return dirs;
}

std::vector<std::string> pixmap_dirs(const char* data_home,
                                     const char* data_dirs, const char* home)
{
    std::vector<std::string> dirs = data_dir_list(data_home, data_dirs, home);
    for (auto& dir : dirs) {
        dir += dir == "/" ? "pixmaps" : "/pixmaps";
    }
    return dirs;
}

int IconTheme::SubDir::distance(int wanted) const
{
    switch (type) {
        case FIXED:
            return std::abs(size - wanted);
        case SCALABLE:
            if (wanted < min_size) {
                return min_size - wanted;
            }
            return wanted > max_size ? wanted - max_size : 0;
        case THRESHOLD:
            break;
    }
    if (wanted < size - threshold) {
        return size - threshold - wanted;
    }
    return wanted > size + threshold ? wanted - size - threshold : 0;
}

IconTheme::IconTheme(std::vector<std::string> bases_,
                     std::vector<std::string> pixmaps_, std::string_view theme):
    bases(std::move(bases_)), pixmaps(std::move(pixmaps_))
{
    // the theme, its parents depth first (in the order of the Inherits key),
    // then hicolor; every theme is read once
    std::vector<std::string> todo{std::string(theme)};
    std::set<std::string> done;
    bool hicolor_added = false;
    while (!todo.empty() || !hicolor_added) {
        if (todo.empty()) {
            todo.emplace_back("hicolor");
            hicolor_added = true;
        }
        const std::string name = std::move(todo.back());
        todo.pop_back();
        if (name.empty() || !done.insert(name).second) {
            continue;
        }
        std::vector<std::string> parents;
        load(name, parents);
        todo.insert(todo.end(), parents.rbegin(), parents.rend());
    }
}

void IconTheme::load(const std::string& name, std::vector<std::string>& parents)
{
    for (const auto& base : bases) {
        const auto groups = read_groups(fs::path(base) / name / "index.theme");
        const auto main = groups.find("Icon Theme");
        if (main == groups.end()) {
            continue;
        }

        Theme theme{name, {}};
        const auto list = main->second.find("Directories");
        for (const auto& dir :
             split_commas(list != main->second.end() ? list->second : "")) {
            const auto it = groups.find(dir);
            if (it == groups.end() || to_int(it->second, "Scale", 1) != 1) {
                continue;
            }
            SubDir sub;
            sub.name = dir;
            sub.size = to_int(it->second, "Size", 0);
            sub.min_size = to_int(it->second, "MinSize", sub.size);
            sub.max_size = to_int(it->second, "MaxSize", sub.size);
            sub.threshold = to_int(it->second, "Threshold", 2);
            const auto type = it->second.find("Type");
            if (type != it->second.end()) {
                if (type->second == "Fixed") {
                    sub.type = SubDir::FIXED;
                } else if (type->second == "Scalable") {
                    sub.type = SubDir::SCALABLE;
                }
            }
            theme.dirs.push_back(std::move(sub));
        }
        themes.push_back(std::move(theme));

        const auto inherits = main->second.find("Inherits");
        if (inherits != main->second.end()) {
            parents = split_commas(inherits->second);
        }
        return; // the first index.theme describes the theme
    }
    if (name == "hicolor") {
        guess_hicolor();
    }
}

void IconTheme::guess_hicolor()
{
    // Icons installed by hand (~/.local/share/icons/hicolor) come without an
    // index.theme, and the system one can be in a directory that is not
    // searched (pkgsrc uses /usr/pkg/share). The directories of hicolor are
    // named by the convention: NxN/apps and scalable/apps.
    std::map<int, SubDir> sizes;
    bool scalable = false;
    for (const auto& base : bases) {
        std::error_code error;
        fs::directory_iterator dirs(fs::path(base) / "hicolor", error);
        for (const fs::directory_iterator end; !error && dirs != end;
             dirs.increment(error)) {
            const std::string dir = dirs->path().filename().string();
            const char* const last = dir.data() + dir.size();
            int width = 0;
            int height = 0;
            const auto first = std::from_chars(dir.data(), last, width);
            if (dir == "scalable") {
                scalable = true;
            } else if (first.ec == std::errc() && first.ptr != last &&
                       *first.ptr == 'x' && width > 0 &&
                       std::from_chars(first.ptr + 1, last, height).ec ==
                               std::errc() &&
                       height == width) {
                SubDir sub;
                sub.name = dir + "/apps";
                sub.size = sub.min_size = sub.max_size = width;
                sub.type = SubDir::FIXED;
                sizes.emplace(width, std::move(sub));
            }
        }
    }

    Theme theme{"hicolor", {}};
    for (auto& [size, sub] : sizes) {
        theme.dirs.push_back(std::move(sub));
    }
    if (scalable) {
        SubDir sub;
        sub.name = "scalable/apps";
        sub.size = 128;
        sub.min_size = 1;
        sub.max_size = 512;
        sub.type = SubDir::SCALABLE;
        theme.dirs.push_back(std::move(sub));
    }
    if (!theme.dirs.empty()) {
        themes.push_back(std::move(theme));
    }
}

bool IconTheme::exists(const std::string& dir, const std::string& file) const
{
    auto it = listings.find(dir);
    if (it == listings.end()) {
        std::set<std::string> names;
        std::error_code error;
        fs::directory_iterator files(dir, error);
        for (const fs::directory_iterator end; !error && files != end;
             files.increment(error)) {
            std::error_code ignored;
            if (fs::is_regular_file(files->status(ignored))) { // follows links
                names.insert(files->path().filename().string());
            }
        }
        it = listings.emplace(dir, std::move(names)).first;
    }
    return it->second.count(file) != 0;
}

std::string IconTheme::find_in(const Theme& theme, std::string_view name,
                               int size) const
{
    // A bitmap of the right size wins, then a vector file (it is drawn in any
    // size), then the closest bitmap.
    std::string bitmap;
    int bitmap_distance = 0;
    std::string vector;
    for (const auto& dir : theme.dirs) {
        const int distance = dir.distance(size);
        // the theme can be in more base directories
        for (const auto& base : bases) {
            for (const auto extension : EXTENSIONS) {
                const bool is_vector = extension == ".svg";
                if (is_vector ? !vector.empty()
                              : (!bitmap.empty() &&
                                 distance >= bitmap_distance)) {
                    continue; // the first of the equally good ones stays
                }
                std::string where = base;
                where += "/";
                where += theme.name;
                where += "/";
                where += dir.name;
                std::string leaf(name);
                leaf += extension;
                if (!exists(where, leaf)) {
                    continue;
                }
                std::string file = where + "/";
                file += leaf;
                if (is_vector) {
                    vector = std::move(file);
                } else if (distance == 0) {
                    return file;
                } else {
                    bitmap = std::move(file);
                    bitmap_distance = distance;
                }
            }
        }
    }
    return vector.empty() ? bitmap : vector;
}

std::string IconTheme::find_uncached(std::string_view name, int size) const
{
    if (name.empty()) {
        return {};
    }
    if (name[0] == '/') {
        return is_file(std::string(name)) ? std::string(name) : std::string();
    }
    name = without_extension(name);

    for (const auto& theme : themes) {
        if (auto file = find_in(theme, name, size); !file.empty()) {
            return file;
        }
    }
    // Files right in the icon directories are not in the specification, but
    // GTK finds them and applications install there (~/.local/share/icons).
    std::vector<std::string> plain = bases;
    plain.insert(plain.end(), pixmaps.begin(), pixmaps.end());
    for (const auto& dir : plain) {
        for (const auto extension : EXTENSIONS) {
            std::string leaf(name);
            leaf += extension;
            if (exists(dir, leaf)) {
                std::string file = dir;
                file += "/";
                file += leaf;
                return file;
            }
        }
    }
    return {};
}

std::string IconTheme::find(std::string_view name, int size) const
{
    auto key = std::make_pair(std::string(name), size);
    if (const auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    std::string file = find_uncached(name, size);
    cache.emplace(std::move(key), file);
    return file;
}

} // namespace tiny::xdg
