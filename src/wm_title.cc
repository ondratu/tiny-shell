#include <X11/cursorfont.h>

#include "wm_title.h"
#include "wm_theme.h"

WMTitle::WMTitle(Display * display, Window parent,
            int x, int y, int width, int height):
        WMWidget(display, parent, x, y, width, height,
                 0, 0x0, WM_WIN_BACKGROUND)
{
    XSetWindowAttributes attrs;
    attrs.win_gravity = NorthWestGravity;
    XChangeWindowAttributes(display, window, CWWinGravity , &attrs);
}

WMTitle::~WMTitle()
{
    XUngrabButton(display, Button1, AnyModifier, window);
    disconnect(ButtonPress);
    disconnect(ButtonRelease);
    disconnect(MotionNotify);
    disconnect(Expose);
}

void WMTitle::set_events()
{
    XGrabButton(            // Click to WM_header
        display, Button1, AnyModifier, window, true,
        ButtonPressMask | ButtonReleaseMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1));

    connect(ButtonPress,
            static_cast<event_signal_t>(&WMTitle::on_button_press));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&WMTitle::on_button_release));
    connect(MotionNotify,
            static_cast<event_signal_t>(&WMTitle::on_motion_notify));
    connect(Expose,
            static_cast<event_signal_t>(&WMTitle::on_expose));
}

void WMTitle::set_title(const std::string &title)
{
    this->title = title;
    // TODO: vydrazdit expose .. nevim jak na to, ale bude to treba :-)
}

void WMTitle::on_button_press(const XEvent &e, void * data)
{
    // XXX: mozna by stalo za to nastavit drag_n_drop a testovat ho
    // v destructoru...
    XGrabPointer(
        display, window, false,
        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1), CurrentTime);

    on_drag_begin(this, e);
}

void WMTitle::on_button_release(const XEvent &e, void * data)
{
    XUngrabPointer(display, CurrentTime);

    on_drag_end(this, e);
}

void WMTitle::on_motion_notify(const XEvent &e, void * data)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent(  // skip penging motion events
                display, event.xmotion.window, MotionNotify, &event))
    {}

    on_drag_motion(this, event);
}

void WMTitle::on_expose(const XEvent &e, void * data)
{
    // TODO write window title
}
