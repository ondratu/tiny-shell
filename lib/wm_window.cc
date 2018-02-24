#include <algorithm>
#include <list>
#include <cstring>

#include "wm_window.h"
#include "wm_theme.h"

WMWindow::WMWindow(Display * display, Window parent, Window child,
                   int x, int y, uint32_t width, uint32_t height):
        WMWidget(display, parent, x, y, width, height,
                 1, WM_WIN_BORDER_COLOR, WM_WIN_BACKGROUND),
        child(child)
{
    hints = XAllocSizeHints();

    {   // Detect window, if is resizable
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
    }

    if (resizable)      // resizable window have some additional widgets
    {
        left_top = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         0,
                         0,
                         WMCorner::Type::LeftTop));
        children.push_back(left_top);

        right_top = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         width-2*WM_WIN_CORNER,
                         0,
                         WMCorner::Type::RightTop));
        children.push_back(right_top);

        left_bottom = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         0,
                         height-2*WM_WIN_BORDER,
                         WMCorner::Type::LeftBottom));
        children.push_back(left_bottom);

        right_bottom = std::shared_ptr<WMCorner>(
            new WMCorner(display, window,
                         width-2*WM_WIN_BORDER,
                         height-2*WM_WIN_BORDER,
                         WMCorner::Type::RightBottom));
        children.push_back(right_bottom);
    }

    title = std::shared_ptr<WMTitle>(
        new WMTitle(display, window,
                    WM_WIN_BORDER, WM_WIN_BORDER,
                    width-2*WM_WIN_BORDER, WM_WIN_HEADER));
    children.push_back(title);

    close_btn = std::shared_ptr<WMCloseButton>(
        new WMCloseButton(display, window,
                    width-WM_WIN_BORDER-WM_WIN_HEADER+1, WM_WIN_BORDER+1,
                    WM_WIN_HEADER-4, WM_WIN_HEADER-4));
    children.push_back(close_btn);
    XSetWindowAttributes attrs;
    attrs.win_gravity = NorthEastGravity;
    XChangeWindowAttributes(display, close_btn->get_window(), CWWinGravity,
                            &attrs);

    if (resizable)
    {
        maxim_btn = std::shared_ptr<WMButton>(
            new WMButton(display, window,
                width-WM_WIN_BORDER-2*WM_WIN_HEADER+1, WM_WIN_BORDER+1,
                WM_WIN_HEADER-4, WM_WIN_HEADER-4));
        children.push_back(maxim_btn);
        attrs.win_gravity = NorthEastGravity;
        XChangeWindowAttributes(display, maxim_btn->get_window(),
                                CWWinGravity, &attrs);
    }

    minim_btn = std::shared_ptr<WMButton>(
        new WMButton(display, window,
            width-WM_WIN_BORDER-3*WM_WIN_HEADER+1, WM_WIN_BORDER+1,
            WM_WIN_HEADER-4, WM_WIN_HEADER-4));
    children.push_back(minim_btn);
    attrs.win_gravity = NorthEastGravity;
    XChangeWindowAttributes(display, minim_btn->get_window(), CWWinGravity,
                            &attrs);

    XSetWindowBorderWidth(display, child, 0);
    XAddToSaveSet(display, child);
    XReparentWindow(display, child, window,
                    WM_WIN_BORDER, WM_WIN_BORDER+WM_WIN_HEADER);
    char * window_name;
    if (XFetchName(display, child, &window_name))
    {
        title->set_text(window_name);
        free(window_name);
    }
}

WMWindow::~WMWindow()
{
    if (hints) {
        XFree(hints);
    }

    XUngrabButton(display, Button1, AnyModifier, window);

    disconnect(ButtonPress);
    disconnect(FocusIn);
    disconnect(FocusOut);
}

WMWindow* WMWindow::create(Display *display, Window parent, Window child)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, child, &attrs);
    auto win = new WMWindow(
        display, parent, child,
        attrs.x-WM_WIN_BORDER,
        attrs.y-WM_WIN_BORDER,
        attrs.width+WM_WIN_CORNER,
        attrs.height+WM_WIN_CORNER+WM_WIN_HEADER);
    win->set_events();
    win->map_all();

    return win;
}

void WMWindow::map_all()
{
    for (auto w: children){
        w->map();
    }

    map();
}

void WMWindow::set_events()
{
    XSelectInput(display, window,
        SubstructureRedirectMask | SubstructureNotifyMask | FocusChangeMask);

    XSelectInput(display, child,
        PropertyChangeMask);

    connect(ButtonPress,
            static_cast<event_signal_t>(&WMWindow::on_button_press));
    connect(FocusIn,
            static_cast<event_signal_t>(&WMWindow::on_focus_in));
    connect(FocusOut,
            static_cast<event_signal_t>(&WMWindow::on_focus_out));
    connect_window(PropertyNotify, child,
            static_cast<event_signal_t>(&WMWindow::on_property_notify));

    if (resizable)
    {
        for (auto corner: {left_top, left_bottom, right_top, right_bottom}){
            corner->on_drag_begin.connect(
                this,
                static_cast<object_signal_t>(&WMWindow::on_corner_drag_begin));
            corner->on_drag_motion.connect(
                this,
                static_cast<object_signal_t>(&WMWindow::on_corner_drag_motion));
        }
    }

    title->on_drag_begin.connect(
        this,
        static_cast<object_signal_t>(&WMWindow::on_title_drag_begin));
    title->on_drag_motion.connect(
        this,
        static_cast<object_signal_t>(&WMWindow::on_title_drag_motion));

    close_btn->on_click.connect(
        this,
        static_cast<object_signal_t>(&WMWindow::on_close_click));
    minim_btn->on_click.connect(
        this,
        static_cast<object_signal_t>(&WMWindow::on_minimize_click));

    for (auto w: children){
        w->set_events();
    }
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
    }
}

void WMWindow::close()
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

void WMWindow::minimize()
{
    printf("minimize\n");
    // TODO: set state
    is_minimized = true;
    unmap();
    XSetInputFocus(display, parent, RevertToPointerRoot, CurrentTime);
}

void WMWindow::restore()
{
    printf("restore\n");
    is_minimized = false;
    map();
}

void WMWindow::on_close_click(WMObject *o, const XEvent &e, void *data)
{
    close();
}

void WMWindow::on_minimize_click(WMObject *o, const XEvent &e, void *data)
{
    minimize();
}

void WMWindow::on_title_drag_begin(WMObject *o, const XEvent &e, void *data)
{
    set_focus();     // check if is not have focus yet

    start_event = e;
    XGetWindowAttributes(display, window, &start_attrs);
}

void WMWindow::on_title_drag_motion(WMObject *o, const XEvent &e, void *data)
{
    int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

    XMoveWindow(display, window,
        start_attrs.x + xdiff, start_attrs.y + ydiff);
}

void WMWindow::on_corner_drag_begin(WMObject *o, const XEvent &e, void * data)
{
    resizing = true;
    start_event = e;
    XGetWindowAttributes(display, window, &start_attrs);
    long int size_retun;
    XGetWMNormalHints(display, child, hints, &size_retun);

    // theoretical, that not change from first check, but...
    if ((size_retun & PMinSize) != PMinSize){
        hints->min_width = start_attrs.width;
        hints->min_height = start_attrs.height;
    }

    if ((size_retun & PMaxSize) != PMaxSize){
        hints->max_width = start_attrs.width;
        hints->max_height = start_attrs.height;
    }
}

void WMWindow::on_corner_drag_motion(WMObject *o, const XEvent &e, void * data)
{
    if (!resizing){
        fprintf(stderr, "Undefined state in WMWindow::corner_motion\n");
        return;     // undefined state
    }

    if (static_cast<WMCorner*>(o)->get_type() == WMCorner::Type::RightBottom) {
        int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
        int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

        int width = start_attrs.width+xdiff;
        int height = start_attrs.height+ydiff;

        // TODO: it exist steps

        if (std::max(hints->min_width+WM_WIN_CORNER, WM_WIN_MIN_WIDTH) > width) {
            width = std::max(hints->min_width+WM_WIN_CORNER, WM_WIN_MIN_WIDTH);
        }
        if ((0 < hints->max_width) && (hints->max_width < width)) {
            width = hints->max_width;
        }

        if (std::max(hints->min_height+WM_WIN_CORNER+WM_WIN_HEADER, WM_WIN_MIN_HEIGHT) > height) {
            height = std::max(hints->min_height+WM_WIN_CORNER+WM_WIN_HEADER, WM_WIN_MIN_HEIGHT);
        }
        if ((0 < hints->max_height) && (hints->max_height < height)) {
            height = hints->max_height;
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
        XResizeWindow(display, window, width, height);
        XResizeWindow(display, child,
                width-WM_WIN_CORNER, height-WM_WIN_CORNER-WM_WIN_HEADER);
        XResizeWindow(display, title->get_window(), // TODO: call title method
                width-WM_WIN_CORNER, WM_WIN_HEADER);
    }
}

void WMWindow::on_button_press(const XEvent &e, void* data)
{
    printf("\ton_click\n");
    set_focus();

    // resend event to child
    XAllowEvents(display, ReplayPointer, e.xbutton.time);

    XUngrabButton(display, Button1, AnyModifier, window);
}

void WMWindow::on_focus_in(const XEvent &e, void* data)
{
    printf("\ton_focus_in\n");
    // TODO: check, if grab is not clen (propagete mask??)
    XUngrabButton(display, Button1, AnyModifier, window);
}

void WMWindow::on_focus_out(const XEvent &e, void* data)
{
    printf("\ton_focus_out\n");
    XGrabButton(            // on click
        display, Button1, AnyModifier, window, true,
        ButtonPressMask,
        GrabModeSync, GrabModeSync,
        None, None);
}

void WMWindow::on_property_notify(const XEvent &e, void *data)
{
    char *atom_name = XGetAtomName(display, e.xproperty.atom);
    if (std::strcmp(atom_name, "WM_NAME") == 0)
    {
        char * window_name;
        if (XFetchName(display, child, &window_name))
        {
            // FIXME: some windows (like urxvt) sends it bad
            // may be just LC_CTYPE... but not work..
            title->set_text(window_name);
            free(window_name);
        }
    }
    XFree(atom_name);
}
