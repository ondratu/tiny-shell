#include "wm/config.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <utility>

#include "ini.h"
#include "text.h"
#include "xdg/application_launch.h"
#include "xdg/dirs.h"

namespace tiny::wm {

namespace {

constexpr double max_scale = 16;

//! The content of a file, nothing when it can't be opened
std::optional<std::string> read_file(const std::string& file)
{
    FILE* in = fopen(file.c_str(), "r");
    if (in == nullptr) {
        return std::nullopt;
    }
    std::string text;
    char buffer[4096];
    size_t count;
    do { // a short read is the end (or an error)
        count = fread(buffer, 1, sizeof buffer, in);
        text.append(buffer, count);
    } while (count == sizeof buffer);
    fclose(in);
    return text;
}

} // namespace

std::optional<double> parse_scale(const std::string& text)
{
    const std::string trimmed(trim(text));
    char* end = nullptr;
    const double scale = strtod(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || *end != '\0' || !(scale > 0) ||
        scale > max_scale) {
        return std::nullopt;
    }
    return scale;
}

std::optional<double> parse_font_size(const std::string& text)
{
    const std::string trimmed(trim(text));
    char* end = nullptr;
    const double size = strtod(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || *end != '\0' || !(size >= min_font_size) ||
        size > max_font_size) {
        return std::nullopt;
    }
    return size;
}

bool valid_font_style(const std::string& text)
{
    return std::none_of(text.begin(), text.end(), [](char c) {
        return std::iscntrl(static_cast<unsigned char>(c)) != 0;
    });
}

bool valid_font(const std::string& text)
{
    const std::string_view font = trim(text);
    return !font.empty() && std::none_of(font.begin(), font.end(), [](char c) {
        return std::iscntrl(static_cast<unsigned char>(c)) != 0;
    });
}

XftChoices xft_choices(XftOption option)
{
    static constexpr XftChoice antialias[] = {
            {"", "Default"}, {"yes", "On"}, {"no", "Off"}};
    static constexpr XftChoice hinting[] = {{"", "Default"},
                                            {"none", "None"},
                                            {"slight", "Slight"},
                                            {"medium", "Medium"},
                                            {"full", "Full"}};
    static constexpr XftChoice rgba[] = {
            {"", "Default"},          {"none", "None (grayscale)"},
            {"rgb", "RGB"},           {"bgr", "BGR"},
            {"vrgb", "Vertical RGB"}, {"vbgr", "Vertical BGR"}};
    switch (option) {
        case XftOption::Antialias:
            return {std::begin(antialias), std::end(antialias)};
        case XftOption::Hinting:
            return {std::begin(hinting), std::end(hinting)};
        case XftOption::Rgba:
            break;
    }
    return {std::begin(rgba), std::end(rgba)};
}

const char* xft_key(XftOption option)
{
    switch (option) {
        case XftOption::Antialias:
            return "antialias";
        case XftOption::Hinting:
            return "hinting";
        case XftOption::Rgba:
            break;
    }
    return "rgba";
}

bool valid_xft(XftOption option, const std::string& text)
{
    const XftChoices choices = xft_choices(option);
    return std::any_of(choices.begin(), choices.end(),
                       [&](const XftChoice& c) { return text == c.value; });
}

XftChoices wallpaper_choices()
{
    static constexpr XftChoice modes[] = {
            {"", "Fill"}, {"center", "Center"}, {"tile", "Tile"}};
    return {std::begin(modes), std::end(modes)};
}

XftChoices dock_choices()
{
    static constexpr XftChoice modes[] = {
            {"", "Auto-hide"}, {"on", "On"}, {"off", "Off"}};
    return {std::begin(modes), std::end(modes)};
}

bool valid_wallpaper_mode(const std::string& text)
{
    const XftChoices choices = wallpaper_choices();
    return text == "fill" ||
           std::any_of(choices.begin(), choices.end(),
                       [&](const XftChoice& c) { return text == c.value; });
}

bool valid_wallpaper_file(const std::string& text)
{
    // a relative path would lead to another file for a program that starts
    // in another directory
    const bool absolute = text.empty() || text[0] == '/' || text == "~" ||
                          text.compare(0, 2, "~/") == 0;
    return absolute && std::none_of(text.begin(), text.end(), [](char c) {
               return std::iscntrl(static_cast<unsigned char>(c)) != 0;
           });
}

std::vector<std::string> config_files(const char* config_home,
                                      const char* config_dirs, const char* home)
{
    std::vector<std::string> files;
    const auto dirs = xdg::config_dir_list(config_home, config_dirs, home);
    // the list starts with the user, the files are read from the other end
    for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) {
        files.push_back(*it + (*it == "/" ? "" : "/") + "tiny-shell.ini");
    }
    return files;
}

std::string user_config_file(const char* config_home, const char* home)
{
    const auto absolute = [](const char* dir) {
        return dir != nullptr && dir[0] == '/';
    };
    std::string dir;
    if (absolute(config_home)) {
        dir = config_home;
    } else if (absolute(home)) {
        dir = std::string(home) + "/.config";
    } else {
        return {};
    }
    return dir + (dir.back() == '/' ? "" : "/") + "tiny-shell.ini";
}

namespace {

//! The ids of a list like `a.desktop;b.desktop`, with no blanks and repeats
std::vector<std::string> split_ids(std::string_view list)
{
    std::vector<std::string> ids;
    for (std::string& id : split(list, ';', true)) {
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(std::move(id));
        }
    }
    return ids;
}

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
        if (const auto scale = parse_scale(*value)) {
            config.scale = *scale;
        } else {
            problem("[theme] scale must be a number above 0 and up to 16");
        }
    }
    if (const auto value = ini.get("theme", "font")) {
        if (valid_font(*value)) {
            config.font = *value;
        } else {
            problem("[theme] font must be the name of a font family");
        }
    }
    if (const auto value = ini.get("theme", "font_size")) {
        if (const auto size = parse_font_size(*value)) {
            config.font_size = *size;
        } else {
            problem("[theme] font_size must be a number from 4 to 96");
        }
    }
    if (const auto value = ini.get("theme", "font_style")) {
        if (valid_font_style(*value)) {
            config.font_style = *value;
        } else {
            problem("[theme] font_style must be the name of a style");
        }
    }
    const std::pair<XftOption, std::string*> xft[] = {
            {XftOption::Antialias, &config.xft_antialias},
            {XftOption::Hinting, &config.xft_hinting},
            {XftOption::Rgba, &config.xft_rgba}};
    for (const auto& [option, target] : xft) {
        if (const auto value = ini.get("xft", xft_key(option))) {
            if (valid_xft(option, *value)) {
                *target = *value;
            } else {
                problem((std::string("[xft] ") + xft_key(option) +
                         " is not one of the values of the option")
                                .c_str());
            }
        }
    }
    if (const auto value = ini.get("theme", "style")) {
        if (!variant_from_name(*value, config.style)) {
            problem("[theme] style must be dark or light");
        }
    }
    if (const auto value = ini.get("wallpaper", "file")) {
        if (valid_wallpaper_file(std::string(trim(*value)))) {
            config.wallpaper_file = std::string(trim(*value));
        } else {
            problem("[wallpaper] file must be an absolute path or start with "
                    "~/");
        }
    }
    if (const auto value = ini.get("wallpaper", "mode")) {
        if (valid_wallpaper_mode(*value)) {
            config.wallpaper_mode = *value == "fill" ? "" : *value;
        } else {
            problem("[wallpaper] mode must be fill, center or tile");
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
            config.terminal_line = *value;
        }
    }
    if (const auto value = ini.get("panel", "clock_format")) {
        if (valid_font_style(*value)) { // no control character
            config.clock_format = *value;
        } else {
            problem("[panel] clock_format must be a text without control "
                    "characters");
        }
    }
    if (const auto value = ini.get("dock", "mode")) {
        if (const auto mode = parse_dock_mode(*value)) {
            config.dock_mode = *mode;
        } else {
            problem("[dock] mode must be autohide, on or off");
        }
    }
    if (const auto value = ini.get("dock", "favorites")) {
        config.favorites = split_ids(*value);
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
        if (auto text = read_file(file)) {
            sources.push_back({file, std::move(*text)});
        }
    }
    return parse_config(sources, problems);
}

namespace {

bool fail(std::string* error, const std::string& what)
{
    if (error) {
        *error = what + ": " + strerror(errno);
    }
    return false;
}

//! Whether a name or a value can be written to a line of the file
bool writable(const IniValue& v)
{
    const auto plain = [](const std::string& text) {
        return text.find_first_of("\r\n") == std::string::npos;
    };
    return plain(v.section) && v.section.find(']') == std::string::npos &&
           !v.key.empty() && plain(v.key) &&
           v.key.find('=') == std::string::npos && plain(v.value);
}

} // namespace

bool save_favorites(const std::string& file,
                    const std::vector<std::string>& favorites,
                    std::string* error)
{
    std::string list;
    for (const std::string& id : favorites) {
        // the characters that would end the value or the list
        if (id.empty() || id.find_first_of(";\r\n") != std::string::npos) {
            if (error) {
                *error = "wrong id of an application: " + id;
            }
            return false;
        }
        list += (list.empty() ? "" : ";") + id;
    }
    return save_values(file, {{"dock", "favorites", list}}, error);
}

bool save_values(const std::string& file, const std::vector<IniValue>& values,
                 std::string* error)
{
    for (const IniValue& v : values) {
        if (!writable(v)) {
            if (error) {
                *error = "wrong setting [" + v.section + "] " + v.key;
            }
            return false;
        }
    }

    Ini ini;
    ini.parse(read_file(file).value_or(""));
    for (const IniValue& v : values) {
        ini.set(v.section, v.key, v.value);
    }

    // the directory of a new file; its parent is the one of the session
    const auto slash = file.rfind('/');
    if (slash != std::string::npos && slash > 0 &&
        mkdir(file.substr(0, slash).c_str(), 0777) != 0 && errno != EEXIST) {
        return fail(error, "can't make the directory of " + file);
    }

    // a symbolic link is followed, so the file it points to is the one that
    // changes, and a file keeps its permissions
    std::string target = file;
    mode_t mode = 0666;
    char* real = realpath(file.c_str(), nullptr);
    if (real != nullptr) {
        target = real;
        free(real);
        struct stat info;
        if (stat(target.c_str(), &info) == 0) {
            mode = info.st_mode & 07777;
        }
    }

    const std::string temporary =
            target + "." + std::to_string(getpid()) + ".tmp";
    FILE* out = fopen(temporary.c_str(), "w");
    if (out == nullptr) {
        return fail(error, "can't write " + temporary);
    }
    const std::string text =
            "# tiny-shell configuration, written by tiny-shell: a comment is "
            "not kept\n\n" +
            ini.dump();
    // fclose writes what is buffered; fsync makes the data safe before the
    // rename shows them as the file
    const bool written =
            fwrite(text.data(), 1, text.size(), out) == text.size() &&
            fflush(out) == 0 && fchmod(fileno(out), mode) == 0 &&
            fsync(fileno(out)) == 0;
    if (fclose(out) != 0 || !written) {
        const int saved = errno;
        std::remove(temporary.c_str());
        errno = saved;
        return fail(error, "can't write " + temporary);
    }
    if (std::rename(temporary.c_str(), target.c_str()) != 0) {
        const int saved = errno;
        std::remove(temporary.c_str());
        errno = saved;
        return fail(error, "can't replace " + target);
    }
    return true;
}

} // namespace tiny::wm
