#include "wm/cursor_theme.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <system_error>

#ifdef TINY_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

namespace tiny::wm {

bool valid_cursor_theme(const std::string& name)
{
    if (name.empty() || name.size() > 128 || name[0] == '.') {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        return c != '/' && static_cast<unsigned char>(c) >= 0x20 && c != 0x7F;
    });
}

std::vector<ResourceChange> cursor_resources(const std::string& theme, int size)
{
    return {{"Xcursor.theme", theme},
            {"Xcursor.size", size > 0 ? std::to_string(size) : std::string()}};
}

std::vector<ResourceChange> cursor_resources(const Config& config)
{
    if (config.cursor_theme.empty() && config.cursor_size == 0) {
        return {};
    }
    return cursor_resources(config.cursor_theme, config.cursor_size);
}

std::vector<std::string>
installed_cursor_themes(const std::vector<std::string>& data_dirs,
                        const char* home)
{
    std::vector<std::string> bases;
    bases.reserve(data_dirs.size() + 1);
    for (const std::string& dir : data_dirs) {
        bases.push_back(dir + "/icons");
    }
    if (home != nullptr && *home != '\0') {
        bases.push_back(std::string(home) + "/.icons");
    }
    std::vector<std::string> rv;
    for (const std::string& base : bases) {
        std::error_code ignored;
        for (const auto& entry :
             std::filesystem::directory_iterator(base, ignored)) {
            const std::string name = entry.path().filename().string();
            if (valid_cursor_theme(name) &&
                std::filesystem::is_directory(entry.path() / "cursors",
                                              ignored)) {
                rv.push_back(name);
            }
        }
    }
    std::sort(rv.begin(), rv.end());
    rv.erase(std::unique(rv.begin(), rv.end()), rv.end());
    return rv;
}

XftChoices cursor_size_choices()
{
    static constexpr XftChoice sizes[] = {
            {"", "System default"}, {"16", "16"}, {"24", "24"}, {"32", "32"},
            {"48", "48"},           {"64", "64"}, {"96", "96"}};
    return {std::begin(sizes), std::end(sizes)};
}

bool valid_cursor_size(const std::string& text)
{
    if (text.empty()) {
        return true;
    }
    char* end = nullptr;
    const long size = strtol(text.c_str(), &end, 10);
    return *end == '\0' && size >= 1 && size <= max_cursor_size;
}

bool has_cursor_themes()
{
#ifdef TINY_XCURSOR
    return true;
#else
    return false;
#endif
}

void apply_cursor_theme([[maybe_unused]] ::Display* display,
                        [[maybe_unused]] const std::string& theme,
                        [[maybe_unused]] int size)
{
#ifdef TINY_XCURSOR
    // nothing set goes back to what the resources and the system say
    XcursorSetTheme(display, theme.empty() ? nullptr : theme.c_str());
    XcursorSetDefaultSize(display, size);
#endif
}

} // namespace tiny::wm
