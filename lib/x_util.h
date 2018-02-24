#include <X11/Xlib.h>

const char * event_to_string(const XEvent e){
    switch (e.type){
        case ButtonPress:
            return "ButtonPress";
        case ButtonRelease:
            return "ButtonRelease";
        case CirculateNotify:
            return "CirculateNotify";
        case CirculateRequest:
            return "CirculateRequest";
        case ClientMessage:
            return "ClientMessage";
        case ColormapNotify:
            return "ColormapNotify";
        case ConfigureNotify:
            return "ConfigureNotify";
        case ConfigureRequest:
            return "ConfigureRequest";
        case CreateNotify:
            return "CreateNotify";
        case DestroyNotify:
            return "DestroyNotify";
        case EnterNotify:
            return "EnterNotify";
        case Expose:
            return "Expose";
        case FocusIn:
            return "FocusIn";
        case FocusOut:
            return "FocusOut";
        case GraphicsExpose:
            return "GraphicsExpose";
        case KeymapNotify:
            return "KeymapNotify";
        case KeyPress:
            return "KeyPress";
        case KeyRelease:
            return "KeyRelease";
        case LeaveNotify:
            return "LeaveNotify";
        case MapNotify:
            return "MapNotify";
        case MappingNotify:
            return "MappingNotify";
        case MapRequest:
            return "MapRequest";
        case MotionNotify:
            return "MotionNotify";
        case NoExpose:
            return "NoExpose";
        case PropertyNotify:
            return "PropertyNotify";
        case ReparentNotify:
            return "ReparentNotify";
        case ResizeRequest:
            return "ResizeRequest";
        case SelectionClear:
            return "SelectionClear";
        case SelectionNotify:
            return "SelectionNotify";
        case SelectionRequest:
            return "SelectionRequest";
        case UnmapNotify:
            return "UnmapNotify";
        case VisibilityNotify:
            return "VisibilityNotify";
        default:
            return "Unknown type";
    }
}

