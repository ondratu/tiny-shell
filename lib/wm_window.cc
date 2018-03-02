#include <algorithm>
#include <cstring>

#include "wm_window.h"

namespace wm {

Window::Window(::Window child, uint32_t width, uint32_t height):
    tiny::Container(width, height+WM_WIN_HEADER),
    child(child),
    header(width, WM_WIN_HEADER),
    shadow(width, height+WM_WIN_HEADER)
{
    hints = XAllocSizeHints();
}

Window::~Window(){
    printf("Window::~Window\n");
    if (hints){
        XFree(hints);
    }

    XUngrabButton(display, Button1, AnyModifier, window);

    disconnect(ButtonPress);
    disconnect(FocusIn);
    disconnect(FocusOut);
    disconnect_window(PropertyNotify, child);
}

Window* Window::create(Display *display, ::Window parent, ::Window child){
    XWindowAttributes attrs;
    XGetWindowAttributes(display, child, &attrs);
    auto win = new Window(child, attrs.width, attrs.height);
    // TODO: move down under panel
    win->realize(display, parent, attrs.x, attrs.y);
    win->map_all();

    return win;
}

void Window::realize(Display *display, ::Window parent, int x, int y)
{
    tiny::Container::realize(display, parent, x, y);

    {   // Detect window, if is resizable
        long int size_retun;
        XGetWMNormalHints(display, child, hints, &size_retun);
        if ((((size_retun & PMinSize) != PMinSize) &&
                    ((size_retun & PMaxSize) != PMaxSize)) ||
                ((hints->min_width == hints->max_width) &&
                 (hints->min_height == hints->min_height) &&
                 (hints->min_width > 0 && hints->min_height > 0)))
        {
            is_resizable = false;
        }
    }

    if (is_resizable){
        shadow.realize(display, parent, x, y);
    }

    add(&header, 0, 0);
    header.push_back(&cls_btn,
            WM_WIN_HEADER_PADDING, WM_WIN_HEADER_PADDING-WM_BTN_BORDER);
    if (is_resizable){
        header.push_back(&max_btn,
                WM_WIN_HEADER_PADDING, WM_WIN_HEADER_PADDING-WM_BTN_BORDER);
    }
    header.push_back(&min_btn,
            WM_WIN_HEADER_PADDING, WM_WIN_HEADER_PADDING-WM_BTN_BORDER);

    XSetWindowBorderWidth(display, child, 0);
    XAddToSaveSet(display, child);
    XReparentWindow(display, child, window, 0, WM_WIN_HEADER);
    char * window_name;
    if (XFetchName(display, child, &window_name))
    {
        header.set_title(window_name);
        free(window_name);
    }
}

void Window::set_events(long mask)
{
    // SubstructureNotifyMask so, wm::Manager can catch UnmapNotify
    tiny::Container::set_events(mask|SubstructureNotifyMask|FocusChangeMask);

    XSelectInput(display, child, PropertyChangeMask);

/*
    XSelectInput(display, window,
        SubstructureRedirectMask | SubstructureNotifyMask | FocusChangeMask);

    minim_btn->on_click.connect(
        this,
        static_cast<object_signal_t>(&WMWindow::on_minimize_click));
    }
*/

    connect(ButtonPress,
            static_cast<tiny::event_signal_t>(&Window::on_button_press));
    connect(FocusIn,
            static_cast<tiny::event_signal_t>(&Window::on_focus_in));
    connect(FocusOut,
            static_cast<tiny::event_signal_t>(&Window::on_focus_out));
    connect_window(PropertyNotify, child,
            static_cast<tiny::event_signal_t>(&Window::on_property_notify));

    header.get_title_box()->on_drag_begin.connect(
        this,
        static_cast<tiny::object_signal_t>(&Window::on_window_drag_begin));
    header.get_title_box()->on_drag_motion.connect(
        this,
        static_cast<tiny::object_signal_t>(&Window::on_window_drag_motion));

    if (is_resizable)
    {
        shadow.on_move_resize_begin.connect(
            this,
            static_cast<tiny::object_signal_t>(&Window::on_move_resize_begin));
        shadow.on_move_resize_motion.connect(
            this,
            static_cast<tiny::object_signal_t>(&Window::on_move_resize_motion));
    }


    cls_btn.on_click.connect(this,
            static_cast<tiny::object_signal_t>(&Window::on_close_click));
}

void Window::map_all(){
    shadow.map_all();
    tiny::Container::map_all();

    // set_focus();
    XRaiseWindow(display, shadow.get_window());     // shadow is lower
    XRaiseWindow(display, window);
}

void Window::set_focus()
{
    XSetInputFocus(display, child, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(display, shadow.get_window());
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

void Window::close()
{
    printf("doing close...");
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

void Window::minimize()
{
    printf("minimize\n");
    // TODO: set state
    is_minimized = true;
    unmap();
    XSetInputFocus(display, parent, RevertToPointerRoot, CurrentTime);
}

void Window::restore()
{
    printf("restore\n");
    is_minimized = false;
    map();
}

void Window::on_close_click(tiny::Object *o, const XEvent &e, void *data){
    close();
}

void Window::on_move_resize_begin(tiny::Object *o, const XEvent &e, void *data)
{
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

void Window::on_move_resize_motion(tiny::Object *o, const XEvent &e, void *data)
{
    uint16_t mask = reinterpret_cast<size_t>(data);
    int xdiff = 0;
    int ydiff = 0;

    if (mask & (tiny::Position::Left|tiny::Position::Right)){
        xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    }
    if (mask & (tiny::Position::Top|tiny::Position::Bottom)){
        ydiff = e.xbutton.y_root - start_event.xbutton.y_root;
    }

    int width = start_attrs.width;
    int height = start_attrs.height;

    if (mask & tiny::Position::Right){
        width += xdiff;
    } else {    // ::Position::Left
        width -= xdiff;
    }

    if (mask &tiny::Position::Bottom){
        height += ydiff;
    } else {    // ::Position::Top
        height -= ydiff;
    }

    if (mask & (tiny::Position::Left|tiny::Position::Right)){
        if (std::max(hints->min_width+WM_WIN_CORNER, WM_WIN_MIN_WIDTH) > width) {
            width = std::max(hints->min_width+WM_WIN_CORNER, WM_WIN_MIN_WIDTH);
        }
        if ((0 < hints->max_width) && (hints->max_width < width)) {
            width = hints->max_width;
        }
    }

    if (mask & (tiny::Position::Top|tiny::Position::Bottom)){
        // TODO: it exist steps
        if (std::max(hints->min_height+WM_WIN_CORNER+WM_WIN_HEADER, WM_WIN_MIN_HEIGHT) > height) {
            height = std::max(hints->min_height+WM_WIN_CORNER+WM_WIN_HEADER, WM_WIN_MIN_HEIGHT);
        }
        if ((0 < hints->max_height) && (hints->max_height < height)) {
            height = hints->max_height;
        }
    }

    if (mask & tiny::Position::Top || mask & tiny::Position::Left) {
        int x = start_attrs.x;
        int y = start_attrs.y;
        if (mask & tiny::Position::Left){
            x += xdiff;
        }
        if (mask & tiny::Position::Top){
            y += ydiff;
        }

        XMoveResizeWindow(display, window, x, y, width, height);
        shadow.move_resize(x, y, width, height);
    } else {
        XResizeWindow(display, window, width, height);
        shadow.resize(width, height);
    }

    XResizeWindow(display, child, width, height-WM_WIN_HEADER);
    header.resize(width, WM_WIN_HEADER);
}

void Window::on_window_drag_begin(tiny::Object *o, const XEvent &e, void *data)
{
    set_focus();     // check if is not have focus yet

    start_event = e;
    XGetWindowAttributes(display, window, &start_attrs);
}

void Window::on_window_drag_motion(tiny::Object *o, const XEvent &e, void *data)
{
    int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

    shadow.move(start_attrs.x + xdiff, start_attrs.y + ydiff);
    XMoveWindow(display, window,
        start_attrs.x + xdiff, start_attrs.y + ydiff);
}

void Window::on_button_press(const XEvent &e, void* data)
{
    printf("\ton_click\n");
    set_focus();

    // resend event to child
    XAllowEvents(display, ReplayPointer, e.xbutton.time);

    XUngrabButton(display, Button1, AnyModifier, window);
}

void Window::on_focus_in(const XEvent &e, void* data)
{
    printf("\ton_focus_in\n");
    // TODO: check, if grab is not clen (propagete mask??)
    XUngrabButton(display, Button1, AnyModifier, window);
}

void Window::on_focus_out(const XEvent &e, void* data)
{
    printf("\ton_focus_out\n");
    XGrabButton(            // on click
        display, Button1, AnyModifier, window, true,
        ButtonPressMask,
        GrabModeSync, GrabModeSync,
        None, None);
}

void Window::on_property_notify(const XEvent &e, void *data)
{
    char *atom_name = XGetAtomName(display, e.xproperty.atom);
    if (std::strcmp(atom_name, "WM_NAME") == 0)
    {
        char * window_name;
        if (XFetchName(display, child, &window_name))
        {
            // FIXME: some windows (like urxvt) sends it bad
            // may be just LC_CTYPE... but not work..
            header.set_title(window_name);
            free(window_name);
        }
    }
    XFree(atom_name);
}

} // namespace wm
