#include <X11/cursorfont.h>

#include "wm_buttons.h"
#include "wm_theme.h"

extern Handlers_t handlers;

WMButton::WMButton(Display * display, Window parent,
                   int x, int y, int width, int height):
        WMWidget(display, parent, x, y, width, height, 1,
                 BUTTON_BG, BUTTON_BG)
{
    XSelectInput(display, window, ExposureMask|EnterWindowMask|LeaveWindowMask);

    XGrabButton(            // on click
        display, Button1, AnyModifier, window, false,
        ButtonReleaseMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand2));

    handlers.set_handler(Expose, window, on_expose,
        static_cast<void*>(this), nullptr);
    handlers.set_handler(EnterNotify, window, on_enter_notify,
        static_cast<void*>(this), nullptr);
    handlers.set_handler(LeaveNotify, window, on_leave_notify,
        static_cast<void*>(this), nullptr);
}

WMButton::~WMButton(){
    XUngrabButton(
        display, Button1, AnyModifier, window);

    handlers.unset_handler(Expose, window);
    handlers.unset_handler(EnterNotify, window);
    handlers.unset_handler(LeaveNotify, window);

    if (on_click_handler){
        handlers.unset_handler(ButtonRelease, window);
    }
}

void WMButton::connect(const std::string &signal, on_event_t handler,
                       void * data)
{
    handlers.set_handler(ButtonRelease, window, on_click,
        static_cast<void *>(this), data);
    on_click_handler = handler;
}

void WMButton::on_click(const XEvent &e, void* w, void* data)
{
    // TODO: check if release is on window, if not, do not call the handler :-)
    auto btn = static_cast<WMButton*>(w);
    btn->on_click_handler(*btn, e, data);
}

void WMButton::on_expose(const XEvent &e, void* w, void* data)
{
    auto btn = static_cast<WMButton*>(w);
    btn->expose(e, data);
}

void WMButton::on_enter_notify(const XEvent &e, void* _this, void*){
    auto btn = static_cast<WMButton*>(_this);
    XSetWindowBorder(btn->display, btn->window, BUTTON_BORDER);
}
void WMButton::on_leave_notify(const XEvent &e, void* _this, void*){
    auto btn = static_cast<WMButton*>(_this);
    XSetWindowBorder(btn->display, btn->window, BUTTON_BG);
}

void WMButton::expose(const XEvent &e, void*)
{}

/* WMCloseButton */
WMCloseButton::WMCloseButton(Display * display, Window parent,
                             int x, int y, int width, int height):
        WMButton(display, parent, x, y, width, height)
{}

WMCloseButton::~WMCloseButton()
{}

void WMCloseButton::expose(const XEvent &e, void *)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, BUTTON_FG);
    XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinBevel);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    XDrawLine(display, window, gc,
        mid_x-3.5, mid_y-3.5, mid_x+3.5, mid_y+3.5);
    XDrawLine(display, window, gc,
        mid_x+3.5, mid_y-3.5, mid_x-3.5, mid_y+3.5);
    XFreeGC(display, gc);
}

/* WMMinimizeButton */
WMMinimizeButton::WMMinimizeButton(Display * display, Window parent,
                                   int x, int y, int width, int height):
        WMButton(display, parent, x, y, width, height)
{}

WMMinimizeButton::~WMMinimizeButton()
{}

void WMMinimizeButton::expose(const XEvent &e, void *)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, BUTTON_FG);
    XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinRound);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    XDrawLine(display, window, gc,
        mid_x-4, mid_y+3.5, mid_x+4, mid_y+3.5);
    XFreeGC(display, gc);
}
