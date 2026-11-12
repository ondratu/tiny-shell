#pragma once

#include <X11/Xlib.h>

namespace tiny::wm {

/**
 * @short Whether this is the first time that the session asks to start its
 * autostart applications; asking again is "no".
 *
 * A window manager that is started again in the same session (it crashed, or
 * the user runs `tiny-shell --replace`) must not start every application a
 * second time. The answer is kept in the property `_TINY_SHELL_AUTOSTART` of
 * the root window, which lives as long as the X server, so the session.
 */
bool claim_autostart(::Display* display, ::Window root);

} // namespace tiny::wm
