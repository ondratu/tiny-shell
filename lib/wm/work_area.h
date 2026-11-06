#pragma once

#include <X11/Xlib.h>

#include <vector>

#include "widget.h"

namespace tiny::wm {

//! Edges of the screen reserved by a dock (_NET_WM_STRUT)
struct Struts {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
};

//! `_NET_WM_STRUT_PARTIAL` of a window, else `_NET_WM_STRUT`; zeros if none.
//! The start and the end of the strut along the edge are not used: the whole
//! edge is reserved.
Struts read_struts(::Window window);

//! The screen without the struts of the docks and of the panel
tiny::Rect compute_work_area(int screen_width, int screen_height,
                             const std::vector<Struts>& docks);

/**
 * @short The work area of one monitor: the monitor without the struts of the
 * docks that reach it.
 *
 * A strut is measured from the edge of the whole screen (`screen_width` x
 * `screen_height`, the root window), as `_NET_WM_STRUT` says: a top strut of 30
 * covers the monitors that start in the first 30 rows of the screen, a monitor
 * below another one is not touched by it.
 */
tiny::Rect compute_monitor_area(const tiny::Rect& monitor, int screen_width,
                                int screen_height,
                                const std::vector<Struts>& docks);

//! The area for the frames whose borders are drawn outside of them
tiny::Rect inner_area(const tiny::Rect& area, int border);

//! Shrink the frame to the area and move it inside
tiny::Rect fit_in_area(tiny::Rect frame, const tiny::Rect& area);

} // namespace tiny::wm
