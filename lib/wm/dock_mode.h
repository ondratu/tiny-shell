#pragma once

#include <optional>
#include <string>

#include "widget.h"

namespace tiny::wm {

/// When the dock is shown.
enum class DockMode {
    AutoHide, //!< hidden while the active window covers it, the pointer at the
              //!< bottom edge of the screen shows it (the default)
    On,       //!< always shown
    Off       //!< never shown
};

/// The mode from the text of a setting: "autohide", "on" or "off" (the case
/// is ignored, "auto" and "always" are other names of two of them); nothing
/// for any other text.
std::optional<DockMode> parse_dock_mode(const std::string& text);

/// The text a setting has for the mode.
const char* dock_mode_name(DockMode mode);

/// Whether two rectangles have a pixel in common.
bool overlaps(const Rect& a, const Rect& b);

/**
 * @short Whether the dock is shown.
 *
 * With `AutoHide` it is hidden when the active window covers a part of it
 * (`covered`) unless the pointer has been brought to the edge of the screen
 * (`revealed`; it stays until the pointer leaves the dock).
 */
bool dock_shown(DockMode mode, bool covered, bool revealed);

} // namespace tiny::wm
