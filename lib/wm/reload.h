#pragma once

#include <string>

#include <X11/Xlib.h>

namespace tiny::wm {

/**
 * @short Ask the tiny-shell that runs on the display to read its settings
 * again (`tiny-shell --reload`).
 *
 * The window manager is found through `_NET_SUPPORTING_WM_CHECK`, its process
 * through `_NET_WM_PID` and `WM_CLIENT_MACHINE` of that window, and it gets
 * SIGHUP. Nothing is sent to a window manager that is not tiny-shell or that
 * runs on another computer. Returns an empty string, or what went wrong.
 */
std::string request_reload(::Display* display, ::Window root);

} // namespace tiny::wm
