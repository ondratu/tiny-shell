#pragma once

#include <string>

#include <X11/Xlib.h>

namespace tiny::wm {

/**
 * @short Ask the tiny-shell that runs on the display to read its settings
 * again (`tiny-shell --reload`).
 *
 * The window manager is found through `_NET_SUPPORTING_WM_CHECK` and its name,
 * and it gets a ClientMessage on the root window (the same as SIGHUP does, but
 * no process is signalled, so it works from another computer as well).
 * Nothing is sent when the window manager is not tiny-shell. Returns an empty
 * string, or what went wrong.
 */
std::string request_reload(::Display* display, ::Window root);

} // namespace tiny::wm
