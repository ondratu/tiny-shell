#pragma once

#include <stdint.h>

#include <vector>

#include <X11/Xlib.h>

#include "widget.h"

namespace tiny::wm {

/**
 * @short The picture under the dock that makes its bar see-through.
 *
 * There is no compositor, so the bar is not blended by the server: the dock
 * takes the part of the wallpaper that is under it (the pixmap of
 * `_XROOTPMAP_ID`, which `tiny-wallpaper` publishes) and blends the color of
 * the bar over it. The icons have `ParentRelative` background, so they show
 * the same picture.
 *
 * `place` is where the dock is on the screen, `bar_top` is the first row of
 * the bar (the rows above it show the wallpaper only), `opacity` is the part
 * of the bar color in percent. Without a wallpaper `fallback` takes its place.
 * Returns the pixmap (the caller frees it), `None` when it can't be made.
 */
Pixmap make_backdrop(::Display* display, ::Window root, const tiny::Rect& place,
                     int bar_top, uint32_t bar_color, int opacity,
                     uint32_t fallback);

//! The pixmap of the wallpaper (`_XROOTPMAP_ID`), `None` when there is none
//! or it is gone
Pixmap root_pixmap(::Display* display, ::Window root);

//! The rectangles that the dock of the half bar shows: the bar (the lower
//! half of the dock `width` x `height`) and the places of the `parts`
std::vector<XRectangle> half_bar_shape(int width, int height,
                                       const std::vector<tiny::Rect>& parts);

} // namespace tiny::wm
