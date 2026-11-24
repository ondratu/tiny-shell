#pragma once

#include <X11/Xlib.h>

#include <string>
#include <vector>

#include "wm/config.h"
#include "wm/xresources.h"

namespace tiny::wm {

//! Is a name of a cursor theme usable: a name of a directory in
//! `icons/` (no slash, no dots at the start, no control characters)
bool valid_cursor_theme(const std::string& name);

//! The biggest `[theme] cursor_size` in pixels
constexpr int max_cursor_size = 256;

/**
 * @short The resources `Xcursor.theme` and `Xcursor.size` for the programs
 * that read the cursor theme from the X resources (Xlib with libXcursor
 * does). What is not set gives lines to remove, so the programs keep what the
 * system says.
 */
std::vector<ResourceChange> cursor_resources(const std::string& theme,
                                             int size);

//! The same for `config`
std::vector<ResourceChange> cursor_resources(const Config& config);

/**
 * @short Make the cursors that Xlib creates from now on follow the theme.
 *
 * With libXcursor (`TINY_XCURSOR`) the theme and the size of the display are
 * set, so `XCreateFontCursor()` draws the shapes of the theme. An empty theme
 * and the size 0 give back what the system says (the resources
 * `Xcursor.*`, so publish them first). Without libXcursor the core
 * cursors of the X server stay and this does nothing.
 */
void apply_cursor_theme(::Display* display, const std::string& theme, int size);

/**
 * @short The names of the cursor themes that are installed: the directories
 * `icons/<name>/cursors` of the data directories and `~/.icons`. Sorted, each
 * name once.
 */
std::vector<std::string>
installed_cursor_themes(const std::vector<std::string>& data_dirs,
                        const char* home);

//! The sizes that a form offers; the first one, "", is the one of the system
XftChoices cursor_size_choices();

//! Is the text "" or a whole number of pixels, 1 to `max_cursor_size`
bool valid_cursor_size(const std::string& text);

//! Is the program built with libXcursor, so that a theme can be set
bool has_cursor_themes();

} // namespace tiny::wm
