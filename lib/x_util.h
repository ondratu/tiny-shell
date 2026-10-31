#pragma once

#include <algorithm>
#include <X11/X.h>
#include <X11/Xlib.h>

namespace tiny {

inline const char* event_to_string(const XEvent e)
{
    switch (e.type) {
        case KeyPress:
            return "KeyPress";
        case KeyRelease:
            return "KeyRelease";
        case ButtonPress:
            return "ButtonPress";
        case ButtonRelease:
            return "ButtonRelease";
        case MotionNotify:
            return "MotionNotify";
        case EnterNotify:
            return "EnterNotify";
        case LeaveNotify:
            return "LeaveNotify";
        case FocusIn:
            return "FocusIn";
        case FocusOut:
            return "FocusOut";
        case KeymapNotify:
            return "KeymapNotify";
        case Expose:
            return "Expose";
        case GraphicsExpose:
            return "GraphicsExpose";
        case NoExpose:
            return "NoExpose";
        case VisibilityNotify:
            return "VisibilityNotify";
        case CreateNotify:
            return "CreateNotify";
        case DestroyNotify:
            return "DestroyNotify";
        case UnmapNotify:
            return "UnmapNotify";
        case MapNotify:
            return "MapNotify";
        case MapRequest:
            return "MapRequest";
        case ReparentNotify:
            return "ReparentNotify";
        case ConfigureNotify:
            return "ConfigureNotify";
        case ConfigureRequest:
            return "ConfigureRequest";
        case GravityNotify:
            return "GravityNotify";
        case ResizeRequest:
            return "ResizeRequest";
        case CirculateNotify:
            return "CirculateNotify";
        case CirculateRequest:
            return "CirculateRequest";
        case PropertyNotify:
            return "PropertyNotify";
        case SelectionClear:
            return "SelectionClear";
        case SelectionRequest:
            return "SelectionRequest";
        case SelectionNotify:
            return "SelectionNotify";
        case ColormapNotify:
            return "ColormapNotify";
        case ClientMessage:
            return "ClientMessage";
        case MappingNotify:
            return "MappingNotify";
        case GenericEvent:
            return "GenericEvent";
        case LASTEvent:
            return "LASTEvent";
        default:
            return "Unknown type";
    }
}

/**
 * @short Return right event window, instead of XEvent.any.window.
 */
::Window window_of_event(const XEvent& e);

/**
 * @short Size for the X requests that take unsigned width or height.
 *
 * Geometry is computed with int. X refuses a zero size (BadValue) and the
 * protocol has 16 bits for it, so the value is limited to 1..65535.
 */
inline unsigned int x_dim(int size)
{
    return static_cast<unsigned int>(std::clamp(size, 1, 65535));
}

/// An int that must not be negative for an X request that takes unsigned.
inline unsigned int x_uint(int value)
{
    return static_cast<unsigned int>(std::max(value, 0));
}

inline XPoint x_point(int x, int y)
{
    XPoint rv;
    rv.x = static_cast<short>(std::clamp(x, -32768, 32767));
    rv.y = static_cast<short>(std::clamp(y, -32768, 32767));
    return rv;
}

void x_grab_key(::Display* display, int keycode, unsigned int modifiers,
                ::Window window);

void x_ungrab_key(::Display* display, int keycode, unsigned int modifiers,
                  ::Window window);

} // namespace tiny
