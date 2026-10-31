#include <X11/extensions/Xrender.h>
#include <algorithm>
#include <cmath>

#include "x_util.h"

namespace tiny {

::Window window_of_event(const XEvent& e)
{
    // Some events has different window to XEvent.xany.window.
    switch (e.type) {
        case KeyPress:
        case KeyRelease:
            return e.xkey.window;
        case ButtonPress:
        case ButtonRelease:
            return e.xbutton.window;
        case MotionNotify:
            return e.xmotion.window;
        case EnterNotify:
        case LeaveNotify:
            return e.xcrossing.window;
        case FocusIn:
        case FocusOut:
            return e.xfocus.window;
        case KeymapNotify:
            return e.xkeymap.window;
        case Expose:
            return e.xexpose.window;
        case GraphicsExpose:
            return e.xgraphicsexpose.drawable;
        case NoExpose:
            return e.xnoexpose.drawable;
        case VisibilityNotify:
            return e.xvisibility.window;
        case CreateNotify:
            return e.xcreatewindow.window; //!< diff
        case DestroyNotify:
            return e.xdestroywindow.window; //!< diff
        case UnmapNotify:
            return e.xunmap.window; //!< diff
        case MapNotify:
            return e.xmap.window; //!< diff
        case MapRequest:
            return e.xmaprequest.window; //!< diff
        case ReparentNotify:
            return e.xreparent.window; //!< diff
        case ConfigureNotify:
            return e.xconfigure.window; //!< diff
        case ConfigureRequest:
            return e.xconfigurerequest.window; //!< diff
        case GravityNotify:
            return e.xgravity.window; //!< diff
        case ResizeRequest:
            return e.xresizerequest.window;
        case CirculateNotify:
            return e.xcirculate.window; //!< diff
        case CirculateRequest:
            return e.xcirculaterequest.window; //!< diff
        case PropertyNotify:
            return e.xproperty.window;
        case SelectionClear:
            return e.xselectionclear.window;
        case SelectionRequest:
            return e.xselectionrequest.requestor; //!< diff
        case SelectionNotify:
            return e.xselection.requestor;
        case ColormapNotify:
            return e.xcolormap.window;
        case ClientMessage:
            return e.xclient.window;
        case MappingNotify:
        default: // GenericEvent and unknown (extension) events
            return None;
    }
}

void x_grab_key(::Display* display, int keycode, unsigned int modifiers,
                ::Window window)
{
    XGrabKey(display, keycode, modifiers, window, True, GrabModeAsync,
             GrabModeAsync);
    XGrabKey(display, keycode, modifiers | Mod2Mask, // Num Lock
             window, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, modifiers | LockMask, // Caps Lock
             window, True, GrabModeAsync, GrabModeAsync);
    // Num Lock + Caps Lock
    XGrabKey(display, keycode, modifiers | Mod2Mask | LockMask, window, True,
             GrabModeAsync, GrabModeAsync);
}

void x_ungrab_key(::Display* display, int keycode, unsigned int modifiers,
                  ::Window window)
{
    // Base without Num or Cups locks
    XUngrabKey(display, keycode, modifiers, window);
    // Num Lock
    XUngrabKey(display, keycode, modifiers | Mod2Mask, window);
    // Caps Lock
    XUngrabKey(display, keycode, modifiers | LockMask, window);
    // Num Lock + Caps Lock
    XUngrabKey(display, keycode, modifiers | Mod2Mask | LockMask, window);
}

} // namespace tiny
