#include <X11/cursorfont.h>

#include "object.h"
#include "theme.h"
#include "wm_window.h"
#include "wm_wrapped.h"
#include "x_util.h"

namespace tiny {
    extern std::shared_ptr<Display> display;
}

namespace wm {

Wrapped::Wrapped(::Window child_arg, ::Window root_arg, uint32_t w, uint32_t h,
        unsigned long functions_arg, unsigned long decorations_arg):
    tiny::Container(w, h+tiny::theme.wm_win_header, "wm_window"),
    Window(child_arg, root_arg, functions_arg),
    decorations(decorations_arg),
    header(w, tiny::theme.wm_win_header),
    shadow(w, h+tiny::theme.wm_win_header)
{}

static int ignore_x_error(::Display *, XErrorEvent *)
{ return 0; }

Wrapped::~Wrapped(){
    // Give the client back to the root, so it survives the frame (it can be
    // gone already, e.g. when it was closed).
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs)) {
        XErrorHandler old = XSetErrorHandler(ignore_x_error);
        XReparentWindow(display, child, parent, attrs.x,
                attrs.y + tiny::theme.wm_win_header);
        XSync(display, False);
        XSetErrorHandler(old);
    }

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

    unsigned long data[2];      /* "suggested" by ICCCM version 1 */

    data[0] = (unsigned long) NormalState;
    data[1] = (unsigned long) None;

    XChangeProperty(tiny::get_display(), win->get_window(),
            tiny::get_display().WM_STATE, tiny::get_display().WM_STATE, 32,
            PropModeReplace, (unsigned char *) data, 2);

    return win;
}

void Wrapped::realize(::Window parent_arg, int x, int y)
{
    update_properties();
    tiny::Container::realize(parent_arg, x, y);

    if (is_resizable()){
        shadow.realize(parent_arg, x, y);
    }

    add(&header, 0, 0);
    header.push_back(&cls_btn,
            header.get_padding(),
            header.get_padding()-cls_btn.get_border());
    if (is_maximizable()){
        header.push_back(&max_btn,
                header.get_padding(),
                header.get_padding()-max_btn.get_border());
    }
    header.push_back(&min_btn,
            header.get_padding(),
            header.get_padding()-min_btn.get_border());

    XSetWindowBorderWidth(display, child, 0);
    XReparentWindow(display, child, window, 0, tiny::theme.wm_win_header);
    header.set_title(wm_name);
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
*/

    header.get_title_box()->on_drag_begin.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_window_drag_begin));
    header.get_title_box()->on_drag_motion.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_window_drag_motion));

    min_btn.on_click.connect(
        reinterpret_cast<tiny::Object*>(this),
        reinterpret_cast<tiny::object_signal_t>(&Wrapped::on_minimize_click));

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

void Wrapped::on_minimize_click(tiny::Object *, const XEvent &, void *){
    minimize();
}

void Wrapped::on_close_click(tiny::Object *, const XEvent &, void *){
    close();
}

void Wrapped::on_maximize_click(tiny::Object *, const XEvent &, void *){
    if (get_maximized()){
        restore();
    } else {
        maximize();
    }
}

void Wrapped::on_move_resize_begin(tiny::Object *, const XEvent &e, void *)
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

void Wrapped::on_move_resize_motion(tiny::Object *, const XEvent &e, void *data)
{
    uint16_t mask = reinterpret_cast<size_t>(data);
    int xdiff = 0;
    int ydiff = 0;
    int mod;

    if (mask & (tiny::Position::Left|tiny::Position::Right)){
        xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
        mod = (hints->width_inc ? xdiff % hints->width_inc : 0);
        if (mod) {
            xdiff -= mod;
        }
    }
    if (mask & (tiny::Position::Top|tiny::Position::Bottom)){
        ydiff = e.xbutton.y_root - start_event.xbutton.y_root;
        mod = (hints->height_inc ? ydiff % hints->height_inc : 0);
        if (mod) {
            ydiff -= mod;
        }
    }

    int new_width = start_attrs.width;
    int new_height = start_attrs.height;

    if (mask & tiny::Position::Right){
        new_width += xdiff;
    } else {    // ::Position::Left
        new_width -= xdiff;
    }

    if (mask &tiny::Position::Bottom){
        new_height += ydiff;
    } else {    // ::Position::Top
        new_height -= ydiff;
    }

    if (mask & (tiny::Position::Left|tiny::Position::Right)){
        if (std::max(hints->min_width+tiny::theme.wm_win_corner,
                     tiny::theme.wm_win_min_width) > (size_t)new_width)
        {
            new_width = std::max(hints->min_width+tiny::theme.wm_win_corner,
                             tiny::theme.wm_win_min_width);
        }
        if ((0 < hints->max_width) && (hints->max_width < new_width)) {
            new_width = hints->max_width;
        }
    }

    if (mask & (tiny::Position::Top|tiny::Position::Bottom)){
        // TODO: it exist steps
        if (std::max(hints->min_height+tiny::theme.wm_win_corner+tiny::theme.wm_win_header,
                     tiny::theme.wm_win_min_height) > (size_t)new_height)
        {
            new_height = std::max(hints->min_height+tiny::theme.wm_win_corner+tiny::theme.wm_win_header,
                              tiny::theme.wm_win_min_height);
        }
        if ((0 < hints->max_height) && (hints->max_height < new_height)) {
            new_height = hints->max_height;
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

        XMoveResizeWindow(display, window, x, y, new_width, new_height);
        shadow.move_resize(x, y, new_width, new_height);
    } else {
        XResizeWindow(display, window, new_width, new_height);
        shadow.resize(new_width, new_height);
    }

    XResizeWindow(display, child, new_width, new_height-tiny::theme.wm_win_header);
    header.resize(new_width, tiny::theme.wm_win_header);
}

void Wrapped::on_window_drag_begin(tiny::Object *o, const XEvent &e, void *data)
{
    Window::on_window_drag_begin(o, e, data);
    XGetWindowAttributes(display, window, &start_attrs);
}

void Wrapped::on_window_drag_motion(tiny::Object *, const XEvent &e, void *)
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
    Window::on_focus_in(e, data);
}

void Wrapped::on_focus_out(const XEvent &e, void* data){
    header.set_disable(true);
    Window::on_focus_out(e, data);
}

void Wrapped::on_property_notify(const XEvent &e, void *data)
{
    Window::on_property_notify(e, data);

    if (e.xproperty.atom == display.WM_NAME ||
            e.xproperty.atom == display._NET_WM_NAME)
    {
        header.set_title(wm_name);
    }
    if (e.xproperty.atom == display.WM_NORMAL_HINTS)
    {
        // SDL on resize moved window relative to root
        ::Window geo_root;
        int x, y;
        unsigned int geo_width, geo_height;
        unsigned int border, depth;
        if (!XGetGeometry(dsp, child, &geo_root,
                &x, &y, &geo_width, &geo_height, &border, &depth))
        {
            TINY_LOG("Can't get window geometry");
            return;
        }

        // when window is moved
        if (x != 0 || y != static_cast<int>(tiny::theme.wm_win_header)){
            XWindowChanges ch;
            ch.x = 0;
            ch.y = tiny::theme.wm_win_header;
            XConfigureWindow(dsp, child, CWX | CWY, &ch);
        }
    }
}

} // namespace
