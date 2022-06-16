#include <X11/cursorfont.h>

#include <algorithm>
#include <cstring>

#include "wm_window.h"

namespace tiny {
    extern std::shared_ptr<Display> display;
}

namespace wm {

Window::Window(::Window child, uint32_t width, uint32_t height):
    tiny::Container(width, height+WM_WIN_HEADER),
    child(child),
    header(width, WM_WIN_HEADER),
    shadow(width, height+WM_WIN_HEADER)
{
    hints = XAllocSizeHints();
    update_protocols();
}

Window::~Window(){
    disconnect_window(PropertyNotify, child);

    if (hints){
        XFree(hints);
    }

    XUngrabButton(display, Button1, AnyModifier, window);

    disconnect(ButtonPress);
    disconnect(ButtonRelease);
    disconnect(MotionNotify);
    disconnect(FocusIn);
    disconnect(FocusOut);
}

Window* Window::create(::Window parent, ::Window child,
                       const XWindowAttributes& attrs)
{
    auto win = new Window(child, attrs.width, attrs.height);
    // TODO: move down under panel
    win->realize(parent, attrs.x, attrs.y);
    win->on_focus_out(XEvent(), nullptr);   // disable and GrabButton
    win->map_all();

    return win;
}

void Window::realize(::Window parent, int x, int y)
{
    update_properties();
    tiny::Container::realize(parent, x, y);

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
        shadow.realize(parent, x, y);
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
    char * wm_name = get_net_wm_name();
    if (!wm_name){
        XFetchName(display, child, &wm_name);
    }
    if (wm_name)
    {
        header.set_title(wm_name);
        XFree(wm_name);
    }
}

void Window::set_events(long mask)
{
    // SubstructureNotifyMask so, wm::Manager can catch UnmapNotify
    tiny::Container::set_events(mask|SubstructureNotifyMask|FocusChangeMask);

    XSelectInput(display, child, PropertyChangeMask);

    XGrabButton(display, Button1, AnyModifier, window, true,
            ButtonPressMask|ButtonReleaseMask,
            GrabModeSync, GrabModeSync,
            None, None);

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
    connect(ButtonRelease,
            static_cast<tiny::event_signal_t>(&Window::on_button_release));
    connect(MotionNotify,
            static_cast<tiny::event_signal_t>(&Window::on_motion_notify));
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

    on_drag_begin.connect(
            this,
            static_cast<tiny::object_signal_t>(&Window::on_window_drag_begin));
    on_drag_motion.connect(
            this,
            static_cast<tiny::object_signal_t>(&Window::on_window_drag_motion));


    cls_btn.on_click.connect(this,
            static_cast<tiny::object_signal_t>(&Window::on_close_click));

    if (is_resizable)
    {
        shadow.on_move_resize_begin.connect(
            this,
            static_cast<tiny::object_signal_t>(&Window::on_move_resize_begin));
        shadow.on_move_resize_motion.connect(
            this,
            static_cast<tiny::object_signal_t>(&Window::on_move_resize_motion));
        max_btn.on_click.connect(this,
                static_cast<tiny::object_signal_t>(&Window::on_maximize_click));
    }
}

void Window::map_all(){
    if (is_resizable){
        shadow.map_all();
    }
    tiny::Container::map_all();

    if (is_resizable){
        XRaiseWindow(display, shadow.get_window());     // shadow is lower
    }
    XRaiseWindow(display, window);
}

void Window::set_focus()
{
    if (is_resizable){
        XRaiseWindow(display, shadow.get_window());
    }
    XRaiseWindow(display, window);
    XSetInputFocus(display, child, RevertToPointerRoot, CurrentTime);

    if (protocols.count(display.WM_TAKE_FOCUS))
    {
        XClientMessageEvent msg;
        msg.message_type = display.WM_PROTOCOLS;
        msg.display = display;
        msg.window = child;
        msg.format = 32;
        msg.data.l[0] = display.WM_TAKE_FOCUS;
        msg.data.l[1] = CurrentTime;

        XSendEvent(display, child, false, NoEventMask, (XEvent*) &msg);
    }
}

void Window::close()
{
    if (protocols.count(display.WM_DELETE_WINDOW))
    {
        XClientMessageEvent msg;
        msg.window = child;
        msg.type = ClientMessage;
        msg.format = 32;
        msg.message_type = display.WM_PROTOCOLS;
        msg.data.l[0] = display.WM_DELETE_WINDOW;
        XSendEvent(display, child, false, NoEventMask, (XEvent*) &msg);
    } else {
        XKillClient(display, child);
    }
}

void Window::minimize()
{
    // TODO: set state
    is_minimized = true;
    unmap();
    XSetInputFocus(display, parent, RevertToPointerRoot, CurrentTime);
}

void Window::maximize()
{
    if (is_maximize){
        return;
    }
    XWindowAttributes win_attrs;
    XGetWindowAttributes(display, window, &win_attrs);
    state_width = width;
    state_height = height;
    state_x = win_attrs.x;
    state_y = win_attrs.y;

    XWindowAttributes root_attrs;   //TODO: get_screen size
    XGetWindowAttributes(display, parent, &root_attrs);

    XSetWindowBorderWidth(display, window, 0);
    XMoveResizeWindow(display, window, 0, 0,
            root_attrs.width, root_attrs.height);
    XResizeWindow(display, child,
            root_attrs.width, root_attrs.height-WM_WIN_HEADER);
    if (is_resizable){
        shadow.move_resize(0, 0, root_attrs.width, root_attrs.height);
    }
    header.resize(root_attrs.width, WM_WIN_HEADER);

    max_btn.set_restore(true);
    is_maximize = true;
}

void Window::restore(int x, int y)
{
    if (is_maximize){
        // TODO: move state_x near to pointer (x), which is on wm_window
        // TODO: move statr_y near to pointer (y), which is on wm_window

        if (is_resizable) {
            shadow.move_resize(0, 0, state_width, state_height);
        }
        header.resize(state_width, WM_WIN_HEADER);
        XResizeWindow(display, child,
                state_width, state_height-WM_WIN_HEADER);
        XMoveResizeWindow(display, window,
                state_x, (y ? y : state_y), state_width, state_height);
        XSetWindowBorderWidth(display, window, 1);

        max_btn.set_restore(false);
        is_maximize = false;
    } else {
        map();
        is_minimized = false;
    }
}

void Window::update_protocols()
{
    protocols.clear();

    Atom* supported;
    int count;
    if (XGetWMProtocols(display, child, &supported, &count) == 0) {
        return;
    }
    for (size_t i =0; i < count; ++i){
        protocols.insert(supported[i]);
    }
    XFree(supported);
}

void Window::update_properties()
{
    properties.clear();

    int count;
    Atom * props = XListProperties(display, child, &count);

    for (int i=0; i < count; ++i){
        properties.insert(props[i]);
        char * prop_name = XGetAtomName(display, props[i]);
        printf(" { Atom } %s\n", prop_name);
        XFree(prop_name);
    }
    XFree(props);
}

char * Window::get_net_wm_name(){
    if (properties.count(display._NET_WM_NAME))
    {
        Atom actual_type;
        int actual_format;
        unsigned long nitems;
        unsigned long leftover;
        unsigned char *data = NULL;
        if (XGetWindowProperty(display, child,
                               display._NET_WM_NAME, 0L, BUFSIZ,
                               false, display.UTF8_STRING,
                               &actual_type, &actual_format,
                               &nitems, &leftover, &data) != Success)
        {
            _net_wm_name = false;
            return nullptr;
        }

        if ((actual_type == display.UTF8_STRING) && (actual_format == 8))
        {
            _net_wm_name = true;
            return reinterpret_cast<char*>(data);
        } else {
            _net_wm_name = false;
            XFree(data);
        }
    }
    return nullptr;
}


void Window::on_close_click(tiny::Object *o, const XEvent &e, void *data){
    close();
}

void Window::on_maximize_click(tiny::Object *o, const XEvent &e, void *data){
    if (is_maximize){
        restore();
    } else {
        maximize();
    }
}

void Window::on_move_resize_begin(tiny::Object *o, const XEvent &e, void *data)
{
    is_maximize = false;
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
    int mod;

    if (mask & (tiny::Position::Left|tiny::Position::Right)){
        xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
        mod = (hints->width_inc ? xdiff % hints->width_inc : 0);
        if (mod) {
            xdiff -= (hints->width_inc - std::abs(mod));
        }
    }
    if (mask & (tiny::Position::Top|tiny::Position::Bottom)){
        ydiff = e.xbutton.y_root - start_event.xbutton.y_root;
        mod = (hints->height_inc ? ydiff % hints->height_inc : 0);
        if (mod) {
            printf("mod: %d, ydiff: %d, height_inc: %d start: %d\n",
                    mod, ydiff, hints->height_inc, start_attrs.height);
            ydiff -= (hints->height_inc - std::abs(mod));
        }
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
    if (is_maximize){
        restore(e.xmotion.x, e.xmotion.y);
    }

    start_event = e;
    XGetWindowAttributes(display, window, &start_attrs);
}

void Window::on_window_drag_motion(tiny::Object *o, const XEvent &e, void *data)
{
    int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

    if (is_resizable){
        shadow.move(start_attrs.x + xdiff, start_attrs.y + ydiff);
    }
    XMoveWindow(display, window,
        start_attrs.x + xdiff, start_attrs.y + ydiff);
}

void Window::on_button_press(const XEvent &e, void* data)
{
    set_focus();        // set_focus is not important :-)

    if (e.xbutton.state & Mod1Mask){
        // unblock the pointer, but not send to child
        XAllowEvents(display, AsyncPointer, e.xbutton.time);
        XGrabPointer(
                display, window, false,
                ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
                GrabModeAsync, GrabModeAsync,
                None, XCreateFontCursor(display, XC_hand1), CurrentTime);
        on_drag_begin(this, e);
        return;
    }

    // resend event to child
    XAllowEvents(display, ReplayPointer, e.xbutton.time);
}

void Window::on_button_release(const XEvent &e, void * data){
    // Stop motion
    XUngrabPointer(display, CurrentTime);
}

void Window::on_motion_notify(const XEvent &e, void * data){
    on_drag_motion(this, e);
}

void Window::on_focus_in(const XEvent &e, void* data){
    header.set_disable(false);
    on_focus(this, e, nullptr);
}

void Window::on_focus_out(const XEvent &e, void* data){
    header.set_disable(true);
}

void Window::on_property_notify(const XEvent &e, void *data)
{
    char *atom_name = XGetAtomName(display, e.xproperty.atom);
    TINY_LOG("on_property_notify %s", atom_name);
    XFree(atom_name);

    if (e.xproperty.atom == display.WM_PROTOCOLS){
        update_protocols();
        return;
    }

    if (e.xproperty.atom == display.WM_NAME && ! _net_wm_name)
    {
        char * wm_name;
        if (XFetchName(display, child, &wm_name))
        {
            header.set_title(wm_name);
            free(wm_name);
        }
        return;
    }

    if (e.xproperty.atom == display._NET_WM_NAME)
    {
        char * wm_name = get_net_wm_name();
        if (wm_name)
        {
           header.set_title(wm_name);
           XFree(wm_name);
        }
        return;
    }
}

} // namespace wm
