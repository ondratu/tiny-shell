#include <X11/cursorfont.h>

#include "wm_title.h"
#include "wm_theme.h"

extern Handlers_t handlers;

WMTitle::WMTitle(Display * display, Window parent,
                 int x, int y, int width, int height):
        WMWidget(display, parent, x, y, width, height,
                 0, 0x0, WM_BACKGROUND)
{
    XGrabButton(            // Click to WM_header
            display, Button1, AnyModifier, window, true,
            ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync,
            None, XCreateFontCursor(display, XC_hand1));

    XSetWindowAttributes attrs;
    attrs.win_gravity = NorthWestGravity;
    XChangeWindowAttributes(display, window, CWWinGravity , &attrs);

    /*
       XGrabButton(            // Click to WM_header
       display, Button3, AnyModifier, header, true,
       ButtonPressMask | ButtonReleaseMask,
       GrabModeAsync, GrabModeAsync,
       None, XCreateFontCursor(display, XC_hand2));
     */
    handlers.set_handler(Expose, window, on_expose,
        static_cast<void*>(this), nullptr);
}

WMTitle::~WMTitle()
{
    XUngrabButton(
        display, Button1, AnyModifier, window);
    handlers.unset_handler(Expose, window);

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

void WMTitle::connect(const std::string &signal, on_event_t handler, void * data)
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

void WMTitle::on_mouse_press(const XEvent &e, void* _this, void* data)
{
    auto title = static_cast<WMTitle*>(_this);
    title->on_mouse_press_handler(*title, e, data);
}

void WMTitle::on_mouse_release(const XEvent &e, void* _this, void* data)
{
    auto title = static_cast<WMTitle*>(_this);
    title->on_mouse_release_handler(*title, e, data);
}

void WMTitle::on_mouse_motion(const XEvent &e, void* _this, void* data)
{
    auto title = static_cast<WMTitle*>(_this);
    title->on_mouse_motion_handler(*title, e, data);
}

void WMTitle::on_expose(const XEvent &e, void* _this, void*data)
{
    // TODO: bottom line in WM_BORDER
}
