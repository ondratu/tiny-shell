#include "wm/dock_mode.h"

#include "text.h"

namespace tiny::wm {

std::optional<DockMode> parse_dock_mode(const std::string& text)
{
    const std::string name = lower(trim(text));
    if (name == "autohide" || name == "auto") {
        return DockMode::AutoHide;
    }
    if (name == "on" || name == "always") {
        return DockMode::On;
    }
    if (name == "off") {
        return DockMode::Off;
    }
    return std::nullopt;
}

const char* dock_mode_name(DockMode mode)
{
    switch (mode) {
        case DockMode::AutoHide:
            return "autohide";
        case DockMode::On:
            return "on";
        case DockMode::Off:
            break;
    }
    return "off";
}

bool overlaps(const Rect& a, const Rect& b)
{
    return a.width > 0 && a.height > 0 && b.width > 0 && b.height > 0 &&
           a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height &&
           b.y < a.y + a.height;
}

bool dock_shown(DockMode mode, bool covered, bool revealed)
{
    switch (mode) {
        case DockMode::AutoHide:
            return !covered || revealed;
        case DockMode::On:
            return true;
        case DockMode::Off:
            break;
    }
    return false;
}

} // namespace tiny::wm
