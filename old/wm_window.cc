#include <X11/cursorfont.h>

#include <algorithm>
#include <time.h>

#include "wm_window.h"
#include "wm_theme.h"

extern Handlers_t handlers;

WMWindow::WMWindow(Display * display, Window parent, Window child,
                   int x, int y, int width, int height):
        WMWidget(display, parent, x, y, width, height,
                 1, WM_BORDER_FG, WM_BACKGROUND),
        child(child), moving(false), resizing(false)
{
    /*
    Window ionly = XCreateWindow(display,
            parent,
            x-100, y-100, width+200, height+200, 0,
            CopyFromParent, InputOnly, CopyFromParent,
            CopyFromParent, 0);
    XMapWindow(display, ionly);
    XDefineCursor(display, ionly,
                  XCreateFontCursor(display, XC_clock));

    Window right = XCreateWindow(display,
            ionly,
            x+width, y, 100, height, 0,
            CopyFromParent, InputOnly, CopyFromParent,
            CopyFromParent, 0);
    XMapWindow(display, right);
    XDefineCursor(display, right,
                  XCreateFontCursor(display, XC_right_side));
    */

    bool resizable = true;
    hints = XAllocSizeHints();
    long int size_retun;
    XGetWMNormalHints(display, child, hints, &size_retun);
    if ((((size_retun & PMinSize) != PMinSize) &&
         ((size_retun & PMaxSize) != PMaxSize)) ||
        ((hints->min_width == hints->max_width) &&
         (hints->min_height == hints->min_height) &&
         (hints->min_width > 0 && hints->min_height > 0)))
    {
        resizable = false;
    }

    if (resizable)
    {
        left_top = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         0,
                         0,
                         WMCorner::Type::LeftTop));
        left_top->connect("on-mouse-press", WMWindow::corner_press,
                static_cast<void*>(this));
        left_top->connect("on-mouse-release", WMWindow::corner_release,
                static_cast<void*>(this));
        left_top->connect("on-mouse-motion", WMWindow::corner_motion,
                static_cast<void*>(this));

        right_top = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         width-4*WM_BORDER,
                         0,
                         WMCorner::Type::RightTop));
        right_top->connect("on-mouse-press", WMWindow::corner_press,
            static_cast<void*>(this));
        right_top->connect("on-mouse-release", WMWindow::corner_release,
            static_cast<void*>(this));
        right_top->connect("on-mouse-motion", WMWindow::corner_motion,
            static_cast<void*>(this));

        left_bottom = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         0,
                         height-4*WM_BORDER,
                         WMCorner::Type::LeftBottom));
        left_bottom->connect("on-mouse-press", WMWindow::corner_press,
                static_cast<void*>(this));
        left_bottom->connect("on-mouse-release", WMWindow::corner_release,
                static_cast<void*>(this));
        left_bottom->connect("on-mouse-motion", WMWindow::corner_motion,
                static_cast<void*>(this));

        right_bottom = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         width-4*WM_BORDER,
                         height-4*WM_BORDER,
                         WMCorner::Type::RightBottom));
        right_bottom->connect("on-mouse-press", WMWindow::corner_press,
            static_cast<void*>(this));
        right_bottom->connect("on-mouse-release", WMWindow::corner_release,
            static_cast<void*>(this));
        right_bottom->connect("on-mouse-motion", WMWindow::corner_motion,
            static_cast<void*>(this));
    }

    XSelectInput(display, window,
        SubstructureRedirectMask | SubstructureNotifyMask | FocusChangeMask);

    handlers.set_handler(ButtonPress, window, on_click,
        static_cast<void*>(this), nullptr);
    handlers.set_handler(FocusIn, window, on_focus_in,
        static_cast<void*>(this), nullptr);
    handlers.set_handler(FocusOut, window, on_focus_out,
        static_cast<void*>(this), nullptr);


    XSetWindowBorderWidth(display, child, 0);
    XAddToSaveSet(display, child);
    XReparentWindow(display, child, window,
                    WM_BORDER, WM_BORDER+WM_HEADER);

    title = std::shared_ptr<WMTitle>(
        new WMTitle(display, window,
                    WM_BORDER, WM_BORDER,           // x, y
                    width-2*WM_BORDER, WM_HEADER));      // width, height
    title->connect("on-mouse-press", WMWindow::title_press,
            static_cast<void*>(this));
    title->connect("on-mouse-release", WMWindow::title_release,
            static_cast<void*>(this));
    title->connect("on-mouse-motion", WMWindow::title_motion,
            static_cast<void*>(this));

    close_btn = std::shared_ptr<WMCloseButton>(
        new WMCloseButton(display, window,
                    width-WM_BORDER-WM_HEADER+1, WM_BORDER+1,    // x, y
                    WM_HEADER-4, WM_HEADER-4));               // width, height
    close_btn->connect("on-click", WMWindow::close_window,
            static_cast<void*>(this));
    XSetWindowAttributes attrs;
    attrs.win_gravity = NorthEastGravity;
    XChangeWindowAttributes(display, close_btn->window, CWWinGravity , &attrs);

    if (resizable)
    {
        maxim_btn = std::shared_ptr<WMButton>(
            new WMButton(display, window,
                     width-WM_BORDER-2*WM_HEADER+1, WM_BORDER+1, // x, y
                     WM_HEADER-4, WM_HEADER-4));              // width, height
        attrs.win_gravity = NorthEastGravity;
        XChangeWindowAttributes(display, maxim_btn->window,
                                CWWinGravity, &attrs);
    }

    minim_btn = std::shared_ptr<WMMinimizeButton>(
        new WMMinimizeButton(display, window,
                     width-WM_BORDER-3*WM_HEADER+1, WM_BORDER+1, // x, y
                     WM_HEADER-4, WM_HEADER-4));              // width, height
    minim_btn->connect("on-click", WMWindow::minimize_window,
            static_cast<void*>(this));
    attrs.win_gravity = NorthEastGravity;
    XChangeWindowAttributes(display, minim_btn->window, CWWinGravity , &attrs);
}

WMWindow* WMWindow::create(Display * display, Window parent, Window child)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, child, &attrs);
    return new WMWindow(display, parent, child,
                        attrs.x-WM_BORDER,
                        attrs.y-WM_BORDER,
                        attrs.width+2*WM_BORDER,
                        attrs.height+2*WM_BORDER+WM_HEADER);
}

WMWindow::~WMWindow()
{
    if (hints) {
        XFree(hints);
    }

    handlers.unset_handler(ButtonPress, window);
    handlers.unset_handler(FocusIn, window);
    handlers.unset_handler(FocusOut, window);
}

void WMWindow::set_focus()
{
    XSetInputFocus(display, child, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(display, window);

    const Atom WM_TAKE_FOCUS = XInternAtom(
        display, "WM_TAKE_FOCUS", false);
    const Atom WM_PROTOCOLS = XInternAtom(
        display, "WM_PROTOCOLS", false);

    Atom* supported;
    int count;
    if (XGetWMProtocols(display, child, &supported, &count) &&
            (std::find(supported, supported + sizeof(Atom)*count,
                       WM_TAKE_FOCUS) != supported + sizeof(Atom)*count))
    {
        XClientMessageEvent msg;
        msg.message_type = WM_PROTOCOLS;
        msg.display = display;
        msg.window = child;
        msg.format = 32;
        msg.data.l[0] = WM_TAKE_FOCUS;
        msg.data.l[1] = CurrentTime;

        XSendEvent(display, child, false, NoEventMask, (XEvent*) &msg);
        printf("WM_TAKE_FOCUS sent...\n");
    } else {
        printf("WM_TAKE_FOCUS not supported...\n");
    }
}

void WMWindow::close_window(WMWidget &w, const XEvent &e, void * data)
{
    static_cast<WMWindow*>(data)->do_close_window();
}

void WMWindow::minimize_window(WMWidget &w, const XEvent &e, void* data)
{
    printf("minimize\n");
    WMWindow* win = static_cast<WMWindow*>(data);
    // TODO: set state
    win->is_minimized = true;
    XUnmapWindow(win->display, win->window);
    XSetInputFocus(win->display, win->parent, RevertToPointerRoot, CurrentTime);
}

void WMWindow::do_close_window()
{
    const Atom WM_DELETE_WINDOW = XInternAtom(
        display, "WM_DELETE_WINDOW", false);
    const Atom WM_PROTOCOLS = XInternAtom(
        display, "WM_PROTOCOLS", false);

    Atom* supported;
    int count;
    if (XGetWMProtocols(display, child, &supported, &count) &&
            (std::find(supported, supported + sizeof(Atom)*count,
                       WM_DELETE_WINDOW) != supported + sizeof(Atom)*count))
    {
        XClientMessageEvent msg;
        msg.window = child;
        msg.type = ClientMessage;
        msg.format = 32;
        msg.message_type = WM_PROTOCOLS;
        msg.data.l[0] = WM_DELETE_WINDOW;
        XSendEvent(display, child, false, NoEventMask, (XEvent*) &msg);
    } else {
        XKillClient(display, child);
    }
}

void WMWindow::title_press(WMTitle &title, const XEvent &e, void * data)
{
    auto w = static_cast<WMWindow*>(data);
    w->set_focus();     // check if is not have focus yet

    w->moving = true;
    w->start_event = e;
    XGetWindowAttributes(w->display, w->window, &w->start_attrs);

    XGrabPointer(        // WM Actions -> Motion with header
        w->display, title.window, false,
        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(w->display, XC_hand1), CurrentTime);
}

void WMWindow::title_release(WMTitle &title, const XEvent &e, void * data)
{
    auto w = static_cast<WMWindow*>(data);
    w->moving = false;
    XUngrabPointer(w->display, CurrentTime);
}

void WMWindow::title_motion(WMTitle &title, const XEvent &e, void * data)
{
    XEvent event = e;
    auto w = static_cast<WMWindow*>(data);
    while (XCheckTypedWindowEvent(  // skip penging motion events
                w->display, event.xmotion.window, MotionNotify, &event))
    {}
    if (w->moving) {
        int xdiff = e.xbutton.x_root - w->start_event.xbutton.x_root;
        int ydiff = e.xbutton.y_root - w->start_event.xbutton.y_root;

        XMoveWindow(w->display, w->window,
            w->start_attrs.x + xdiff, w->start_attrs.y + ydiff);
    }
}

void WMWindow::corner_press(WMWidget &w, const XEvent &e, void * data)
{
    auto ww = static_cast<WMWindow*>(data);

    ww->resizing = true;
    ww->start_event = e;
    XGetWindowAttributes(ww->display, ww->window, &ww->start_attrs);
    long int size_retun;
    XGetWMNormalHints(ww->display, ww->child, ww->hints, &size_retun);

    if ((size_retun & PMinSize) != PMinSize){
        ww->hints->min_width = ww->start_attrs.width;
        ww->hints->min_height = ww->start_attrs.height;
    }

    if ((size_retun & PMaxSize) != PMaxSize){
        ww->hints->max_width = ww->start_attrs.width;
        ww->hints->max_height = ww->start_attrs.height;
    }

    XGrabPointer(        // WM Actions -> Motion with header
        ww->display, w.window, false,
        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
        GrabModeAsync, GrabModeAsync,
        None, None, CurrentTime);
}

void WMWindow::corner_release(WMWidget &w, const XEvent &e, void * data)
{
    auto ww = static_cast<WMWindow*>(data);
    ww->moving = false;
    XUngrabPointer(ww->display, CurrentTime);
}

void WMWindow::corner_motion(WMWidget &w, const XEvent &e, void * data)
{
    XEvent event = e;
    auto ww = static_cast<WMWindow*>(data);
    while (XCheckTypedWindowEvent(  // skip penging motion events
                ww->display, event.xmotion.window, MotionNotify, &event))
    {}

    if (!ww->resizing){
        fprintf(stderr, "Undefined state in WMWindow::corner_motion\n");
        return;     // undefined state
    }

    if (static_cast<WMCorner&>(w).type == WMCorner::Type::RightBottom) {
        int xdiff = e.xbutton.x_root - ww->start_event.xbutton.x_root;
        int ydiff = e.xbutton.y_root - ww->start_event.xbutton.y_root;

        int width = ww->start_attrs.width+xdiff;
        int height = ww->start_attrs.height+ydiff;

        // TODO: it exist steps

        if (std::max(ww->hints->min_width+2*WM_BORDER, MIN_WIDTH) > width) {
            width = std::max(ww->hints->min_width+2*WM_BORDER, MIN_WIDTH);
        }
        if ((0 < ww->hints->max_width) && (ww->hints->max_width < width)) {
            width = ww->hints->max_width;
        }

        if (std::max(ww->hints->min_height+2*WM_BORDER+WM_HEADER, MIN_HEIGHT) > height) {
            height = std::max(ww->hints->min_height+2*WM_BORDER+WM_HEADER, MIN_HEIGHT);
        }
        if ((0 < ww->hints->max_height) && (ww->hints->max_height < height)) {
            height = ww->hints->max_height;
        }

        /*
        XMoveResizeWindow(ww->display, ww->window,
                ww->start_attrs.x, ww->start_attrs.y,
                width, height);

        XMoveResizeWindow(ww->display, ww->child,
                WM_BORDER-1,
                WM_BORDER+WM_HEADER,
                width-2*WM_BORDER,
                height-2*WM_BORDER-WM_HEADER);

        XMoveResizeWindow(ww->display, ww->title->window,
                WM_BORDER,
                WM_BORDER,
                width-2*WM_BORDER,
                WM_HEADER);
        */
        XResizeWindow(ww->display, ww->window, width, height);
        XResizeWindow(ww->display, ww->child,
                width-2*WM_BORDER, height-2*WM_BORDER-WM_HEADER);
        XResizeWindow(ww->display, ww->title->window,
                width-2*WM_BORDER, WM_HEADER);
    }

}

void WMWindow::on_click(const XEvent &e, void* w, void* data)
{
    printf("\ton_click\n");
    auto win = static_cast<WMWindow*>(w);
    win->set_focus();

    XAllowEvents(win->display, ReplayPointer, e.xbutton.time);

    XUngrabButton(win->display, Button1, AnyModifier, win->window);
    }

void WMWindow::on_focus_in(const XEvent &e, void* w, void* data)
{
    printf("\ton_focus_in\n");
    auto win = static_cast<WMWindow*>(w);
    // TODO: check, if grab is not clen (propagete mask??)
    XUngrabButton(win->display, Button1, AnyModifier, win->window);
}

void WMWindow::on_focus_out(const XEvent &e, void* w, void* data)
{
    printf("\ton_focus_out\n");
    auto win = static_cast<WMWindow*>(w);
    XGrabButton(            // on click
        win->display, Button1, AnyModifier, win->window, true,
        ButtonPressMask,
        GrabModeSync, GrabModeSync,
        None, None);

}
