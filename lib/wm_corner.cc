#include <X11/cursorfont.h>

#include "wm_corner.h"
#include "wm_theme.h"

WMCorner::WMCorner(Display * display, Window parent,
                   int x, int y, Type type):
        WMWidget(display, parent, x, y, WM_WIN_CORNER*2, WM_WIN_CORNER*2,
                 0, 0x0, WM_WIN_BACKGROUND)
{
    set_type(type);
}

WMCorner::~WMCorner()
{
    XUngrabButton(display, Button1, AnyModifier, window);

    disconnect(ButtonPress);
    disconnect(ButtonRelease);
    disconnect(MotionNotify);
}

void WMCorner::set_events()
{
    XGrabButton(            // Click to WM_header
        display, Button1, AnyModifier, window, true,
        ButtonPressMask | ButtonReleaseMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1));

    connect(ButtonPress,
            static_cast<event_signal_t>(&WMCorner::on_button_press));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&WMCorner::on_button_release));
    connect(MotionNotify,
            static_cast<event_signal_t>(&WMCorner::on_motion_notify));
}

void WMCorner::set_type(Type type)
{
    this->type = type;
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
}

void WMCorner::on_button_press(const XEvent &e, void *data)
{
    XGrabPointer(
        display, window, false,
        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
        GrabModeAsync, GrabModeAsync,
        None, None, CurrentTime);

    on_drag_begin(this, e);
}

void WMCorner::on_button_release(const XEvent &e, void *data)
{
    XUngrabPointer(display, CurrentTime);

    on_drag_end(this, e);
}

void WMCorner::on_motion_notify(const XEvent &e, void *data)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent(  // skip penging motion events
            display, event.xmotion.window, MotionNotify, &event))
    {}

    on_drag_motion(this, event);
}
