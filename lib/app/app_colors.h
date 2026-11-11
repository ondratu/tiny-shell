#pragma once

#include <X11/Xlib.h>

#include <cstdint>

#include "style.h"

namespace tiny::app {

/**
 * @short The colors of the small programs of the shell (tiny-settings,
 * tiny-fonts, tiny-about), 0xRRGGBB.
 *
 * They draw their windows themselves, so they take the colors from here
 * instead of from the widgets of the toolkit.
 */
struct AppColors {
    uint32_t background; //!< the window and its rows
    uint32_t sidebar;    //!< the list of the sections
    uint32_t field;      //!< an entry and a list
    uint32_t text;
    uint32_t dim;      //!< a hint, a disabled text
    uint32_t rule;     //!< the lines and the borders
    uint32_t accent;   //!< the border of the focused field
    uint32_t selected; //!< a chosen row
    uint32_t error;    //!< what is wrong
};

AppColors app_colors(Variant variant);

/**
 * @short The variant that a program looks in.
 *
 * What the manager of settings of the display tells (XSETTINGS: tiny-shell
 * writes it from its variant, so it is also right after a switch of the theme
 * that is not in the file yet), else `configured`, the style of the files.
 */
Variant variant_in_use(::Display* display, int screen, Variant configured);

} // namespace tiny::app
