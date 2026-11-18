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

/// How high the bar under the icons of the dock is.
enum class DockBar {
    Full,  //!< the bar is as high as the dock (the default)
    Half,  //!< the bar is as high as a half of the dock, the icons stick out
           //!< above it
    Hidden //!< there is no bar, only the icons
};

/// The bar from the text of a setting: "full", "half" or "none" (the case is
/// ignored);
/// nothing for any other text.
std::optional<DockBar> parse_dock_bar(const std::string& text);

/// The text a setting has for the bar.
const char* dock_bar_name(DockBar bar);

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
