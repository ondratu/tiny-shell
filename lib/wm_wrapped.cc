#include <X11/cursorfont.h>

#include "wm_wrapped.h"
#include "x_util.h"

namespace tiny {
    extern std::shared_ptr<Display> display;
}

namespace wm {

Wrapped::Wrapped(::Window child, ::Window root, uint32_t width, uint32_t height,
        unsigned long functions, unsigned long decorations):
    tiny::Container(width, height+tiny::theme.wm_win_header, "wm_window"),
    Window(child, root, functions),
    decorations(decorations),
    header(width, tiny::theme.wm_win_header),
    shadow(width, height+tiny::theme.wm_win_header)
{}

Wrapped::~Wrapped(){
    tiny::Container::disconnect_window(PropertyNotify, child);

    XUngrabButton(display, Button1, AnyModifier, window);
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_F4),
            Mod1Mask, window);

    disconnect(ButtonPress);
    disconnect(ButtonRelease);
    disconnect(MotionNotify);
    disconnect(FocusIn);
    disconnect(FocusOut);
}

Window* Wrapped::create(::Window root, ::Window child,
                       const XWindowAttributes& attrs,
                       unsigned long functions, unsigned long decorations)
{
    auto win = new Wrapped(child, root, attrs.width, attrs.height,
                           functions, decorations);
    // TODO: move down under panel
    win->realize(root, attrs.x, attrs.y);
    win->on_focus_out(XEvent(), nullptr);   // disable and GrabButton
    win->map_all();

    return win;
}

void Wrapped::realize(::Window parent, int x, int y)
{
    update_properties();
    tiny::Container::realize(parent, x, y);

    if (is_resizable()){
        shadow.realize(parent, x, y);
    }

    add(&header, 0, 0);
    header.push_back(&cls_btn,
            tiny::theme.wm_header.padding_width,
            tiny::theme.wm_header.padding_width-get_border());
    if (is_maximizable()){
        header.push_back(&max_btn,
                tiny::theme.wm_header.padding_width,
                tiny::theme.wm_header.padding_width-get_border());
    }
    header.push_back(&min_btn,
            tiny::theme.wm_header.padding_width,
            tiny::theme.wm_header.padding_width-get_border());

    XSetWindowBorderWidth(display, child, 0);
    XReparentWindow(display, child, window, 0, tiny::theme.wm_win_header);
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

void Wrapped::set_events(long mask)
{
    // SubstructureNotifyMask so, wm::Manager can catch UnmapNotify
    tiny::Container::set_events(
            mask
            |SubstructureNotifyMask|FocusChangeMask);
    Window::set_events();


/*
    XSelectInput(display, window,
        SubstructureRedirectMask | SubstructureNotifyMask | FocusChangeMask);

    minim_btn->on_click.connect(
        this,
        static_cast<object_signal_t>(&WMWindow::on_minimize_click));
    }
*/

    connect(FocusIn,
            reinterpret_cast<tiny::event_signal_t>(&Wrapped::on_focus_in));
    connect(FocusOut,
            reinterpret_cast<tiny::event_signal_t>(&Wrapped::on_focus_out));

    header.get_title_box()->on_drag_begin.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_window_drag_begin));
    header.get_title_box()->on_drag_motion.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_window_drag_motion));


    cls_btn.on_click.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_close_click));

    if (is_resizable())
    {
        shadow.on_move_resize_begin.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_move_resize_begin));
        shadow.on_move_resize_motion.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_move_resize_motion));
        max_btn.on_click.connect(
                reinterpret_cast<tiny::Object*>(this),
                reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_maximize_click));
    }
}

void Wrapped::map_all(){
    if (is_resizable()){
        shadow.map_all();
    }
    tiny::Container::map_all();

    if (is_resizable()){
        XRaiseWindow(display, shadow.get_window());     // shadow is lower
    }
    XRaiseWindow(display, window);
}

void Wrapped::set_focus()
{
    if (is_resizable()){
        XRaiseWindow(display, shadow.get_window());
    }
    XRaiseWindow(display, window);
    Window::set_focus();
}


void Wrapped::minimize()
{
    // TODO: set state
    set_minimized(true);
    unmap();
    XSetInputFocus(display, parent, RevertToPointerRoot, CurrentTime);
}

void Wrapped::maximize()
{
    if (get_maximized()){
        return;
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(dsp, window, &attrs);
    state.x = attrs.x;
    state.y = attrs.y;
    state.width = width;
    state.height = height;

    XGetWindowAttributes(display, root, &attrs);
    XSetWindowBorderWidth(display, window, 0);

    XMoveResizeWindow(display, window, 0, 0, attrs.width, attrs.height);
    XResizeWindow(display, child,
            attrs.width, attrs.height-tiny::theme.wm_win_header);

    if (is_resizable()){
        shadow.move_resize(0, 0, attrs.width, attrs.height);
    }
    header.resize(attrs.width, tiny::theme.wm_win_header);
    max_btn.set_restore(true);
    set_maximized(true);
}

void Wrapped::restore(int x, int y)
{
    if (get_maximized()){
        // TODO: stay in desktop
        x = x ? (x - state.width/2): state.x;
        y = y ? (y - state.height/2) : state.y;

        if (is_resizable()) {
            shadow.move_resize(x, y, state.width, state.height);
        }
        header.resize(state.width, tiny::theme.wm_win_header);
        XResizeWindow(display, child,
                state.width, state.height-tiny::theme.wm_win_header);
        XMoveResizeWindow(display, window,
                x, y, state.width, state.height);
        XSetWindowBorderWidth(display, window, 1);

        max_btn.set_restore(false);
        set_maximized(false);
    } else {
        map();
        set_minimized(false);
    }
}


char * Wrapped::get_net_wm_name(){
    if (properties.count(display._NET_WM_NAME))
    {
        Atom actual_type;
        int actual_format;
        unsigned long nitems;
        unsigned long leftover;
        unsigned char *data = NULL;
        //TINY_LOG("XGetWindowProperty... on %lx", child);
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


void Wrapped::on_close_click(tiny::Object *o, const XEvent &e, void *data){
    close();
}

void Wrapped::on_maximize_click(tiny::Object *o, const XEvent &e, void *data){
    if (get_maximized()){
        restore();
    } else {
        maximize();
    }
}

void Wrapped::on_move_resize_begin(tiny::Object *o, const XEvent &e, void *data)
{
    set_maximized(false);
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

void Wrapped::on_move_resize_motion(tiny::Object *o, const XEvent &e, void *data)
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
        if (std::max(hints->min_width+tiny::theme.wm_win_corner,
                     tiny::theme.wm_win_min_width) > (size_t)width)
        {
            width = std::max(hints->min_width+tiny::theme.wm_win_corner,
                             tiny::theme.wm_win_min_width);
        }
        if ((0 < hints->max_width) && (hints->max_width < width)) {
            width = hints->max_width;
        }
    }

    if (mask & (tiny::Position::Top|tiny::Position::Bottom)){
        // TODO: it exist steps
        if (std::max(hints->min_height+tiny::theme.wm_win_corner+tiny::theme.wm_win_header,
                     tiny::theme.wm_win_min_height) > (size_t)height)
        {
            height = std::max(hints->min_height+tiny::theme.wm_win_corner+tiny::theme.wm_win_header,
                              tiny::theme.wm_win_min_height);
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

    XResizeWindow(display, child, width, height-tiny::theme.wm_win_header);
    header.resize(width, tiny::theme.wm_win_header);
}

void Wrapped::on_window_drag_begin(tiny::Object *o, const XEvent &e, void *data)
{
    Window::on_window_drag_begin(o, e, data);
    XGetWindowAttributes(display, window, &start_attrs);
}

void Wrapped::on_window_drag_motion(tiny::Object *o, const XEvent &e, void *data)
{
    int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

    if (is_resizable()){
        shadow.move(start_attrs.x + xdiff, start_attrs.y + ydiff);
    }
    XMoveWindow(display, window,
        start_attrs.x + xdiff, start_attrs.y + ydiff);
}

void Wrapped::on_focus_in(const XEvent &e, void* data){
    header.set_disable(false);
    on_focus((Window*)this, e, nullptr);
}

void Wrapped::on_focus_out(const XEvent &e, void* data){
    header.set_disable(true);
}

void Wrapped::on_property_notify(const XEvent &e, void *data)
{
    Window::on_property_notify(e, data);

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

} // namespace
