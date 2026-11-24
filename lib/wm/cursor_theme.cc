#include "wm/cursor_theme.h"

#ifdef TINY_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

namespace tiny::wm {

bool valid_cursor_theme(const std::string& name)
{
    if (name.empty() || name.size() > 128 || name[0] == '.') {
        return false;
    }
    for (const char c : name) {
        if (c == '/' || static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
            return false;
        }
    }
    return true;
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
    if (!theme.empty()) {
        XcursorSetTheme(display, theme.c_str());
    }
    if (size > 0) {
        XcursorSetDefaultSize(display, size);
    }
#endif
}

} // namespace tiny::wm
