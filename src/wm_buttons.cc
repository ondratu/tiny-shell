#include <X11/cursorfont.h>

#include "wm_buttons.h"
#include "wm_theme.h"

WMButton::WMButton(Display *display, Window parent,
                   int x, int y, uint32_t width, uint32_t height):
        WMWidget(display, parent, x, y, width, height,
                 WM_BTN_B, WM_BTN_BG, WM_BTN_BG)
{}

WMButton::~WMButton()
{
    XUngrabButton(display, Button1, AnyModifier, window);
    disconnect(EnterNotify);
    disconnect(LeaveNotify);
    disconnect(ButtonRelease);
}

void WMButton::set_events()
{
    XSelectInput(display, window, EnterWindowMask|LeaveWindowMask);
    XGrabButton(display, Button1, AnyModifier, window, false,
                ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync,
                None, XCreateFontCursor(display, XC_hand2));

    connect(EnterNotify,
            static_cast<event_signal_t>(&WMButton::on_enter_notify));
    connect(LeaveNotify,
            static_cast<event_signal_t>(&WMButton::on_leave_notify));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&WMButton::on_button_release));
}

void WMButton::on_enter_notify(const XEvent &e, void *){
    XSetWindowBorder(display, window, WM_BTN_BC);
}

void WMButton::on_leave_notify(const XEvent &e, void *){
    XSetWindowBorder(display, window, WM_BTN_BG);
}


void WMButton::on_button_release(const XEvent &e, void * data){
    // TODO: check if release is on window, if not do not call the handler
    if (on_click){
        on_click(this, e);
    }
}


WMCloseButton::WMCloseButton(Display *display, Window parent,
        int x, int y, uint32_t width, uint32_t height):
    WMButton(display, parent, x, y, width, height)
{}

WMCloseButton::~WMCloseButton()
{
    disconnect(ExposureMask);
}

void WMCloseButton::set_events()
{
    WMButton::set_events();
    XSelectInput(display, window, ExposureMask|EnterWindowMask|LeaveWindowMask);

    connect(Expose,
            static_cast<event_signal_t>(&WMCloseButton::on_expose));
}

void WMCloseButton::on_expose(const XEvent &e, void *data)
{
    printf("expose ...\n");
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, WM_BTN_FG_ENE);
    XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinBevel);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    XDrawLine(display, window, gc,
        mid_x-3.5, mid_y-3.5, mid_x+3.5, mid_y+3.5);
    XDrawLine(display, window, gc,
        mid_x+3.5, mid_y-3.5, mid_x-3.5, mid_y+3.5);
    XFreeGC(display, gc);

}
