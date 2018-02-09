#include <X11/cursorfont.h>

#include "wm_corner.h"
#include "wm_theme.h"

extern Handlers_t handlers;

WMCorner::WMCorner(Display * display, Window parent,
                   int x, int y, Type type):
        WMWidget(display, parent, x, y, WM_BORDER*4, WM_BORDER*4,
                 0, 0x0, WM_BACKGROUND), type(type)
{
    XSetWindowAttributes attrs;
        switch (type){
        case Type::LeftTop:
            XDefineCursor(display, window,
                  XCreateFontCursor(display, XC_top_left_corner));
            attrs.win_gravity = NorthWestGravity;
            break;
        case Type::LeftBottom:
            XDefineCursor(display, window,
                  XCreateFontCursor(display, XC_bottom_left_corner));
            attrs.win_gravity = SouthWestGravity;
            break;
        case Type::RightTop:
            XDefineCursor(display, window,
                  XCreateFontCursor(display, XC_top_right_corner));
            attrs.win_gravity = NorthEastGravity;
            break;
        case Type::RightBottom:
            XDefineCursor(display, window,
                  XCreateFontCursor(display, XC_bottom_right_corner));
            attrs.win_gravity = SouthEastGravity;
    }

    XChangeWindowAttributes(display, window, CWWinGravity , &attrs);

    XGrabButton(            // Click to WM_header
            display, Button1, AnyModifier, window, true,
            ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync,
            None, XCreateFontCursor(display, XC_hand1));
}

WMCorner::~WMCorner()
{
    XUngrabButton(
        display, Button1, AnyModifier, window);

    if (on_mouse_press_handler){
        handlers.unset_handler(ButtonPress, window);
    }
    if (on_mouse_release_handler){
        handlers.unset_handler(ButtonRelease, window);
    }
    if (on_mouse_motion_handler){
        handlers.unset_handler(MotionNotify, window);
    }
}

void WMCorner::connect(const std::string &signal, on_event_t handler, void * data)
{
    if (signal == "on-mouse-press"){
        handlers.set_handler(ButtonPress, window, on_mouse_press,
            static_cast<void *>(this), data);
        on_mouse_press_handler = handler;
    } else if (signal == "on-mouse-release"){
        handlers.set_handler(ButtonRelease, window, on_mouse_release,
            static_cast<void *>(this), data);
        on_mouse_release_handler = handler;
    } else if (signal == "on-mouse-motion"){
        handlers.set_handler(MotionNotify, window, on_mouse_motion,
            static_cast<void *>(this), data);
        on_mouse_motion_handler = handler;
    } else {
        fprintf(stderr, "Not supported signal %s\n", signal.c_str());
    }
}

void WMCorner::on_mouse_press(const XEvent &e, void* w, void* data)
{
    auto o = static_cast<WMCorner*>(w);
    o->on_mouse_press_handler(*o, e, data);
}

void WMCorner::on_mouse_release(const XEvent &e, void* w, void* data)
{
    auto o = static_cast<WMCorner*>(w);
    o->on_mouse_release_handler(*o, e, data);
}

void WMCorner::on_mouse_motion(const XEvent &e, void* w, void* data)
{
    auto o = static_cast<WMCorner*>(w);
    o->on_mouse_motion_handler(*o, e, data);
}
