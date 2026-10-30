#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <climits>
#include <memory>
#include <new>

#include "object.h"
#include "wm_window.h"
#include "x_util.h"

namespace wm {

namespace {

// Limits of the hints on one axis; ICCCM lets min and base stand for each other.
struct Axis {
    int min;
    int max; // 0: not limited
    int base;
    int inc; // 0: any size
};

Axis axis_of(const XSizeHints& hints, bool horizontal)
{
    const int min = horizontal ? hints.min_width : hints.min_height;
    const int max = horizontal ? hints.max_width : hints.max_height;
    const int base = horizontal ? hints.base_width : hints.base_height;
    const int inc = horizontal ? hints.width_inc : hints.height_inc;
    const bool has_min = hints.flags & PMinSize;
    const bool has_base = hints.flags & PBaseSize;

    Axis axis;
    axis.min = has_min ? min : 0;
    axis.base = has_base ? base : 0;
    if (!has_min && has_base) {
        axis.min = base;
    }
    if (!has_base && has_min) {
        axis.base = min;
    }
    axis.max = (hints.flags & PMaxSize) ? max : 0;
    axis.inc = (hints.flags & PResizeInc) ? std::max(inc, 0) : 0;
    return axis;
}

} // namespace

int fit_client_size(const XSizeHints& hints, bool horizontal, int size)
{
    const Axis axis = axis_of(hints, horizontal);

    if (axis.max > 0) {
        size = std::min(size, axis.max);
    }
    if (axis.inc > 0) {
        size = axis.base + (size - axis.base) / axis.inc * axis.inc;
    }
    if (size < axis.min) {
        // the first size of the grid that is not under the minimum
        size = axis.inc > 0 ? size + (axis.min - size + axis.inc - 1) /
                                              axis.inc * axis.inc
                            : axis.min;
    }
    return size;
}

int client_size_units(const XSizeHints& hints, bool horizontal, int size)
{
    const Axis axis = axis_of(hints, horizontal);
    return axis.inc > 0 ? (size - axis.base) / axis.inc : size;
}

bool has_resize_increment(const XSizeHints& hints)
{
    return (hints.flags & PResizeInc) &&
           (hints.width_inc > 1 || hints.height_inc > 1);
}

Window::Window(::Window child_arg, ::Window root_arg,
               unsigned long functions_arg):
    child(child_arg),
    root(root_arg),
    dsp(tiny::get_display()),
    functions(functions_arg)
{
    hints = XAllocSizeHints();
    if (hints == nullptr) {
        throw std::bad_alloc();
    }

    update_protocols();
    update_properties();

    if (properties.count(dsp._NET_WM_STATE)) {
        update_wm_states();
    }
    if (properties.count(dsp.WM_NORMAL_HINTS)) {
        update_normal_hints();
    }

    if (Window::get_maximized() && (state.width == 0 || state.height == 0)) {
        if (hints->flags & PMinSize) {
            state.width = hints->min_width;
            state.height = hints->min_height;
        }

        XWindowAttributes attrs;
        XGetWindowAttributes(dsp, root_arg,
                             &attrs); //!< TODO: get screen size instead
        state.width = std::max(state.width, attrs.width / 3 * 2);
        state.height = std::max(state.height, attrs.height / 3 * 2);

        if (hints->flags & PMaxSize) {
            state.width = std::min(state.width, hints->max_width);
            state.height = std::min(state.height, hints->max_height);
        }
        if (hints->flags & PAspect) {
            // TODO: fix ratio if is set
            TINY_LOG("Aspect ratio problem...");
        }
    }

    update_wm_name();

    if (properties.count(dsp._NET_WM_ICON)) {
        update_wm_icon();
    }
    if (not icon && properties.count(dsp.WM_HINTS)) {
        XIconSize* icon_size = XAllocIconSize();
        icon_size->min_width = tiny::theme.wm_icon;
        icon_size->min_height = tiny::theme.wm_icon;
        icon_size->max_width = tiny::theme.wm_icon;
        icon_size->max_height = tiny::theme.wm_icon;
        // Request the best icon
        XSetIconSizes(dsp, child_arg, icon_size, 1);
        XFree(icon_size);
        update_wm_hints();
    }
}

Window::~Window()
{
    if (hints) {
        XFree(hints);
    }
    if (icon) {
        XFreePixmap(dsp, icon);
    }
    if (icon_mask) {
        XFreePixmap(dsp, icon_mask);
    }
}

Window* Window::create(::Window root, ::Window child, const XWindowAttributes&,
                       unsigned long functions)
{
    const tiny::Display& display = tiny::get_display();
    auto* win = new Window(child, root, functions);

    XMapWindow(display, child);
    XSetWindowBorderWidth(display, child, 1);
    XRaiseWindow(display, child);

    win->set_events();
    // TODO: move down under panel
    return win;
}

void Window::set_minimized(bool minimized)
{
    if (minimized) {
        wm_states |= WMState::HIDDEN;
    } else {
        wm_states &= static_cast<unsigned long>(~WMState::HIDDEN);
    }

    publish_wm_states();
}

void Window::set_maximized(bool maximized)
{
    if (maximized) {
        wm_states |= WMState::MAXIMIZED_VERT;
        wm_states |= WMState::MAXIMIZED_HORZ;
    } else {
        wm_states &= static_cast<unsigned long>(~WMState::MAXIMIZED_VERT);
        wm_states &= static_cast<unsigned long>(~WMState::MAXIMIZED_HORZ);
    }

    publish_wm_states();
}

void Window::publish_wm_states()
{
    std::vector<Atom> atoms;
    get_wm_states(atoms);

    XChangeProperty(dsp, child, dsp._NET_WM_STATE, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(atoms.data()),
                    static_cast<int>(atoms.size()));
}

void Window::get_wm_states(std::vector<Atom>& atoms)
{
    atoms.clear();

    if (wm_states & WMState::MODAL) {
        atoms.push_back(dsp._NET_WM_STATE_MODAL);
    }
    if (wm_states & WMState::STICKY) {
        atoms.push_back(dsp._NET_WM_STATE_STICKY);
    }
    if (wm_states & WMState::MAXIMIZED_VERT) {
        atoms.push_back(dsp._NET_WM_STATE_MAXIMIZED_VERT);
    }
    if (wm_states & WMState::MAXIMIZED_HORZ) {
        atoms.push_back(dsp._NET_WM_STATE_MAXIMIZED_HORZ);
    }
    if (wm_states & WMState::SHADED) {
        atoms.push_back(dsp._NET_WM_STATE_SHADED);
    }
    if (wm_states & WMState::SKIP_TASKBAR) {
        atoms.push_back(dsp._NET_WM_STATE_SKIP_TASKBAR);
    }
    if (wm_states & WMState::SKIP_PAGER) {
        atoms.push_back(dsp._NET_WM_STATE_SKIP_PAGER);
    }
    if (wm_states & WMState::HIDDEN) {
        atoms.push_back(dsp._NET_WM_STATE_HIDDEN);
    }
    if (wm_states & WMState::FULLSCREEN) {
        atoms.push_back(dsp._NET_WM_STATE_FULLSCREEN);
    }
    if (wm_states & WMState::ABOVE) {
        atoms.push_back(dsp._NET_WM_STATE_ABOVE);
    }
    if (wm_states & WMState::BELOW) {
        atoms.push_back(dsp._NET_WM_STATE_BELOW);
    }
    if (wm_states & WMState::DEMANDS_ATTENTION) {
        atoms.push_back(dsp._NET_WM_STATE_DEMANDS_ATTENTION);
    }
    if (wm_states & WMState::FOCUSED) {
        atoms.push_back(dsp._NET_WM_STATE_FOCUSED);
    }
}

std::string_view get_wm_icon()
{
    return "";
}

void Window::set_focus()
{
    if (get_minimized()) {
        restore();
    }
    XSetInputFocus(dsp, child, RevertToPointerRoot, CurrentTime);

    if (protocols.count(dsp.WM_TAKE_FOCUS)) {
        XClientMessageEvent msg = {};
        msg.type = ClientMessage;
        msg.message_type = dsp.WM_PROTOCOLS;
        msg.display = dsp;
        msg.window = child;
        msg.format = 32;
        msg.data.l[0] = static_cast<long>(dsp.WM_TAKE_FOCUS);
        msg.data.l[1] = CurrentTime;

        XSendEvent(dsp, child, false, NoEventMask,
                   reinterpret_cast<XEvent*>(&msg));
    }
    XRaiseWindow(dsp, child);

    wm_states |= WMState::FOCUSED;

    publish_wm_states();
}

void Window::return_focus()
{
    wm_states &= static_cast<unsigned long>(~WMState::FOCUSED);

    publish_wm_states();
}

void Window::close()
{
    if (protocols.count(dsp.WM_DELETE_WINDOW)) {
        XClientMessageEvent msg = {};
        msg.window = child;
        msg.type = ClientMessage;
        msg.format = 32;
        msg.message_type = dsp.WM_PROTOCOLS;
        msg.data.l[0] = static_cast<long>(dsp.WM_DELETE_WINDOW);
        XSendEvent(dsp, child, false, NoEventMask,
                   reinterpret_cast<XEvent*>(&msg));
    } else {
        XKillClient(dsp, child);
    }
}

void Window::minimize()
{
    if (get_minimized()) {
        return;
    }

    XIconifyWindow(dsp, child, 0);
    set_minimized(true);
}

void Window::maximize()
{
    if (get_maximized()) {
        return;
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(dsp, child, &attrs);
    state.x = attrs.x;
    state.y = attrs.y;
    state.width = attrs.width;
    state.height = attrs.height;

    XGetWindowAttributes(dsp, root, &attrs); //!< TODO: get screen size instead
                                             // TODO: move under the panel
    XMoveResizeWindow(dsp, child, 0, 0, tiny::x_uint(attrs.width),
                      tiny::x_uint(attrs.height));
    set_maximized(true);
}

void Window::restore(int x, int y)
{
    if (get_maximized()) {
        // TODO: stay in desktop
        x = x ? (x - state.width / 2) : state.x;
        y = y ? (y - state.height / 2) : state.y;

        XMoveResizeWindow(dsp, child, x, y, tiny::x_uint(state.width),
                          tiny::x_uint(state.height));
        XSetWindowBorderWidth(dsp, child, 1);
        set_maximized(false);
    } else {
        XMapWindow(dsp, child);
        set_minimized(false);
    }
}

void Window::update_protocols()
{
    protocols.clear();

    Atom* supported;
    int count;
    if (XGetWMProtocols(dsp, child, &supported, &count) == 0) {
        return;
    }
    for (size_t i = 0; i < static_cast<size_t>(count); ++i) {
        protocols.insert(supported[i]);
    }
    XFree(supported);
}

void Window::update_properties()
{
    properties.clear();

    int count;
    Atom* props = XListProperties(dsp, child, &count);

    for (int i = 0; i < count; ++i) {
        properties.insert(props[i]);
    }
    XFree(props);
}

void Window::update_normal_hints()
{
    /*
     * USPosition|USSize|PPosition|PSize|PMinSize|PMaxSize|PResizeInc|PAspect
     * PBaseSize|PWinGravity
     */
    long supplied; // fields Xlib understands, the client's are in hints->flags
    XGetWMNormalHints(dsp, child, hints, &supplied);
    // No limits means no restriction (ICCCM); only equal limits fix the size
    if (hints->flags & PMinSize && hints->flags & PMaxSize &&
        (hints->min_width == hints->max_width) &&
        (hints->min_height == hints->max_height)) {
        functions &= static_cast<unsigned long>(~MotifWMHints::FUNC_ALL);
        functions &= static_cast<unsigned long>(~MotifWMHints::FUNC_RESIZE);
        functions &= static_cast<unsigned long>(~MotifWMHints::FUNC_MAXIMIZE);
    }
}

void Window::update_wm_hints()
{
    XWMHints* wm_hints = XGetWMHints(dsp, child);
    if (wm_hints == nullptr) {
        return;
    }
    if (!(wm_hints->flags & IconPixmapHint)) {
        XFree(wm_hints);
        return; // No Icon in WMHints
    }

    ::Window geo_root;
    int x;
    int y; //!< unused for pixmap
    unsigned int icon_width;
    unsigned int icon_height;
    unsigned int icon_border;
    unsigned int depth; //!< unused dor pixmap
    if (!XGetGeometry(dsp, wm_hints->icon_pixmap, &geo_root, &x, &y,
                      &icon_width, &icon_height, &icon_border, &depth)) {
        TINY_LOG("Can't read geometry from icon!");
        XFree(wm_hints);
        return;
    }
    const int iw = static_cast<int>(icon_width);
    const int ih = static_cast<int>(icon_height);
    const int ib = static_cast<int>(icon_border);

    if (icon) { // On update
        XFreePixmap(dsp, icon);
        icon = 0;
    }
    if (icon_mask) { // On update
        XFreePixmap(dsp, icon_mask);
        icon_mask = 0;
    }

    if (iw == tiny::theme.wm_icon || ih == tiny::theme.wm_icon) {
        icon = XCreatePixmap(dsp, geo_root, tiny::x_uint(tiny::theme.wm_icon),
                             tiny::x_uint(tiny::theme.wm_icon),
                             tiny::x_uint(XDefaultDepth(
                                     dsp, 0))); // TODO: Screen instead of 0

        GC gc = XCreateGC(dsp, icon, 0, nullptr);
        XCopyArea(dsp, wm_hints->icon_pixmap, icon, gc, ib + wm_hints->icon_x,
                  ib + wm_hints->icon_y, tiny::x_uint(tiny::theme.wm_icon),
                  tiny::x_uint(tiny::theme.wm_icon),
                  tiny::theme.wm_icon / 2 - iw / 2,
                  tiny::theme.wm_icon / 2 - ih / 2);
        XFreeGC(dsp, gc);

        if (wm_hints->flags & IconMaskHint) {
            icon_mask = XCreatePixmap(dsp, geo_root,
                                      tiny::x_uint(tiny::theme.wm_icon),
                                      tiny::x_uint(tiny::theme.wm_icon), 1);

            GC mask_gc = XCreateGC(dsp, icon_mask, 0, nullptr);
            XCopyArea(dsp, wm_hints->icon_mask, icon_mask, mask_gc,
                      ib + wm_hints->icon_x, ib + wm_hints->icon_y,
                      tiny::x_uint(tiny::theme.wm_icon),
                      tiny::x_uint(tiny::theme.wm_icon),
                      tiny::theme.wm_icon / 2 - iw / 2,
                      tiny::theme.wm_icon / 2 - ih / 2);
            XFreeGC(dsp, mask_gc);
        }

    } else {
        icon = resize_pixmap(dsp, geo_root, wm_hints->icon_pixmap, icon_width,
                             icon_height, tiny::x_uint(tiny::theme.wm_icon),
                             tiny::x_uint(tiny::theme.wm_icon),
                             tiny::x_uint(XDefaultDepth(dsp, 0)));
        if (wm_hints->flags & IconMaskHint) {
            icon_mask = resize_pixmap(dsp, geo_root, wm_hints->icon_mask,
                                      icon_width, icon_height,
                                      tiny::x_uint(tiny::theme.wm_icon),
                                      tiny::x_uint(tiny::theme.wm_icon), 1);
        }
    }

    XFree(wm_hints);
}

void Window::update_wm_states()
{
    wm_states = 0L;

    Atom returned_type;
    int size;
    unsigned long nitems;
    unsigned long bytes_left;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(dsp, child, dsp._NET_WM_STATE, 0L,
                           static_cast<long>(ULONG_MAX), false, XA_ATOM,
                           &returned_type, &size, &nitems, &bytes_left,
                           &data) != Success ||
        !nitems) {
        // No _NET_WM_STATE
        if (data) {
            XFree(data);
        }
        return;
    }

    char* atom_name;
    const Atom* atoms = reinterpret_cast<Atom*>(data);
    for (unsigned long i = 0; i < nitems; i++) {
        if (atoms[i] == dsp._NET_WM_STATE_MODAL) {
            wm_states |= WMState::MODAL;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_STICKY) {
            wm_states |= WMState::STICKY;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_MAXIMIZED_VERT) {
            wm_states |= WMState::MAXIMIZED_VERT;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_MAXIMIZED_HORZ) {
            wm_states |= WMState::MAXIMIZED_HORZ;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_SHADED) {
            wm_states |= WMState::SHADED;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_SKIP_TASKBAR) {
            wm_states |= WMState::SKIP_TASKBAR;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_SKIP_PAGER) {
            wm_states |= WMState::SKIP_PAGER;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_HIDDEN) {
            wm_states |= WMState::HIDDEN;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_FULLSCREEN) {
            wm_states |= WMState::FULLSCREEN;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_ABOVE) {
            wm_states |= WMState::ABOVE;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_BELOW) {
            wm_states |= WMState::BELOW;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_DEMANDS_ATTENTION) {
            wm_states |= WMState::DEMANDS_ATTENTION;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_FOCUSED) {
            wm_states |= WMState::FOCUSED;
            continue;
        }

        atom_name = XGetAtomName(dsp, atoms[i]);
        TINY_LOG("Unsupported _NET_WM_STATE: %s", atom_name);
        XFree(atom_name);
    }
    XFree(data);
}

void Window::update_wm_name()
{
    is_net_wm_name = false;

    if (properties.count(dsp._NET_WM_NAME)) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems;
        unsigned long leftover;
        unsigned char* data = nullptr;
        if (XGetWindowProperty(dsp, child, dsp._NET_WM_NAME, 0L, BUFSIZ, false,
                               dsp.UTF8_STRING, &actual_type, &actual_format,
                               &nitems, &leftover, &data) == Success) {
            if ((actual_type == dsp.UTF8_STRING) && (actual_format == 8)) {
                wm_name = reinterpret_cast<char*>(data);
            }
            is_net_wm_name = true;
            XFree(data);
        }
    }
    if (wm_name.empty()) {
        char* data;
        if (XFetchName(dsp, child, &data)) {
            wm_name = data;
            XFree(data);
        }
    }
}

void Window::update_wm_icon_name()
{
    if (properties.count(dsp._NET_WM_ICON_NAME)) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems;
        unsigned long leftover;
        unsigned char* data = nullptr;
        if (XGetWindowProperty(dsp, child, dsp._NET_WM_ICON_NAME, 0L, BUFSIZ,
                               false, dsp.UTF8_STRING, &actual_type,
                               &actual_format, &nitems, &leftover,
                               &data) == Success) {
            if ((actual_type == dsp.UTF8_STRING) && (actual_format == 8)) {
                wm_icon_name = reinterpret_cast<char*>(data);
            }
            XFree(data);
        }
    }
    if (wm_icon_name.empty()) {
        char* data;
        if (XGetIconName(dsp, child, &data)) {
            wm_icon_name = data;
            XFree(data);
        }
    }
}

void Window::update_wm_icon()
{
    if (properties.count(dsp._NET_WM_ICON)) {
        Atom returned_type;
        int size;
        unsigned long nitems;
        unsigned long bytes_left;
        uint8_t* data = nullptr;
        if (XGetWindowProperty(dsp, child, dsp._NET_WM_ICON, 0L,
                               static_cast<long>(ULONG_MAX), false, XA_CARDINAL,
                               &returned_type, &size, &nitems, &bytes_left,
                               &data) != Success) {
            return;
        }

        // Find best size
        std::map<unsigned long, size_t> icons;
        size_t pos = 0;
        size_t icon_size = 0;
        unsigned long width = 0;
        unsigned long height = 0;
        // Yes, it can really be 64bit (unsigned long) not 32bit!
        auto* words = reinterpret_cast<unsigned long*>(data);
        while (pos < nitems) {
            width = words[pos];
            height = words[pos + 1];
            icon_size = width * height;
            icons[width] = pos;
            pos += 2 + icon_size; // width+height+icon_size
        }

        for (auto it : icons) { // Find the best icon size
            pos = it.second;
            if (it.first >= static_cast<unsigned long>(tiny::theme.wm_icon)) {
                break;
            }
        }

        width = words[pos];
        height = words[pos + 1];

        if (icon) { // On update
            XFreePixmap(dsp, icon);
        }
        if (icon_mask) { // On update
            XFreePixmap(dsp, icon_mask);
        }

        icon = XCreatePixmap(dsp, root, static_cast<unsigned>(width),
                             static_cast<unsigned>(height),
                             tiny::x_uint(XDefaultDepth(
                                     dsp, 0))); // TODO: Screen instead of 0
        GC gc = XCreateGC(dsp, icon, 0, nullptr);

        icon_mask = XCreatePixmap(dsp, root, static_cast<unsigned>(width),
                                  static_cast<unsigned>(height), 1);
        GC gc_mask = XCreateGC(dsp, icon_mask, 0, nullptr);
        XSetForeground(dsp, gc_mask, 1);

        pos += 2; //!< shift pos to starting data
        uint32_t color;
        uint8_t* pixel;
        // format is RGBA + 4 zeros on 64bit
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                pixel = reinterpret_cast<uint8_t*>(&words[pos + y * width + x]);
                color = static_cast<uint32_t>(pixel[0] * pixel[3] / 255) << 0;
                color |= static_cast<uint32_t>(pixel[1] * pixel[3] / 255) << 8;
                color |= static_cast<uint32_t>(pixel[2] * pixel[3] / 255) << 16;

                XSetForeground(dsp, gc, color);
                XDrawPoint(dsp, icon, gc, static_cast<int>(x),
                           static_cast<int>(y));
                if (color) {
                    XDrawPoint(dsp, icon_mask, gc_mask, static_cast<int>(x),
                               static_cast<int>(y));
                }
            }
        }

        const auto icon_px = static_cast<unsigned long>(tiny::theme.wm_icon);
        if ((width != icon_px) && (height != icon_px)) {
            Pixmap orig = icon;
            icon = resize_pixmap(dsp, root, orig, static_cast<unsigned>(width),
                                 static_cast<unsigned>(height),
                                 tiny::x_uint(tiny::theme.wm_icon),
                                 tiny::x_uint(tiny::theme.wm_icon),
                                 tiny::x_uint(XDefaultDepth(dsp, 0)));
            XFreePixmap(dsp, orig);

            orig = icon_mask;
            icon_mask =
                    resize_pixmap(dsp, root, orig, static_cast<unsigned>(width),
                                  static_cast<unsigned>(height),
                                  tiny::x_uint(tiny::theme.wm_icon),
                                  tiny::x_uint(tiny::theme.wm_icon), 1);
            XFreePixmap(dsp, orig);
        }

        XFreeGC(dsp, gc);
        XFreeGC(dsp, gc_mask);
        XFree(data);
    }
}

void Window::on_window_drag_begin(tiny::Object*, const XEvent& e, void*)
{
    set_focus(); // check if is not have focus yet
    if (get_maximized()) {
        restore(e.xmotion.x, e.xmotion.y);
    }

    start_event = e;
    XGetWindowAttributes(dsp, child, &start_attrs);
}

void Window::on_window_drag_motion(tiny::Object*, const XEvent& e, void*)
{
    const int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    const int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

    XMoveWindow(dsp, child, start_attrs.x + xdiff, start_attrs.y + ydiff);
}

void Window::on_button_press(const XEvent& e, void*)
{
    set_focus(); // set_focus is not important :-)

    if (e.xbutton.state & Mod1Mask) {
        // unblock the pointer, but not send to child
        XAllowEvents(dsp, AsyncPointer, e.xbutton.time);
        XGrabPointer(dsp, child, false,
                     ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
                     GrabModeAsync, GrabModeAsync, None,
                     XCreateFontCursor(dsp, XC_hand1), CurrentTime);
        on_drag_begin(this, e);
        return;
    }

    // resend event to child
    XAllowEvents(dsp, ReplayPointer, e.xbutton.time);
}

void Window::on_button_release(const XEvent&, void*)
{
    // Stop motion
    XUngrabPointer(dsp, CurrentTime);
}

void Window::on_client_message(const XEvent& e, void*)
{
    if (e.xclient.message_type == dsp._NET_WM_STATE) {
        // client want be maximized
        if (e.xclient.data.l[0] == tiny::Display::_NET_WM_STATE_ADD) {
            if (static_cast<Atom>(e.xclient.data.l[1]) ==
                        dsp._NET_WM_STATE_MAXIMIZED_VERT &&
                static_cast<Atom>(e.xclient.data.l[2]) ==
                        dsp._NET_WM_STATE_MAXIMIZED_HORZ) {
                maximize();
                return;
            }

            char* atom_name =
                    XGetAtomName(dsp, static_cast<Atom>(e.xclient.data.l[1]));
            TINY_LOG("Unhandled WM_STATE_ADD request %s", atom_name);
            XFree(atom_name);
        }

        if (e.xclient.data.l[0] == tiny::Display::_NET_WM_STATE_REMOVE) {
            if (static_cast<Atom>(e.xclient.data.l[1]) ==
                        dsp._NET_WM_STATE_MAXIMIZED_VERT &&
                static_cast<Atom>(e.xclient.data.l[2]) ==
                        dsp._NET_WM_STATE_MAXIMIZED_HORZ) {
                restore();
                return;
            }

            char* atom_name =
                    XGetAtomName(dsp, static_cast<Atom>(e.xclient.data.l[1]));
            TINY_LOG("Unhandled WM_STATE_REMOVE request %s", atom_name);
            XFree(atom_name);
        }

        if (e.xclient.data.l[0] == tiny::Display::_NET_WM_STATE_TOGGLE) {
            char* atom_name =
                    XGetAtomName(dsp, static_cast<Atom>(e.xclient.data.l[1]));
            TINY_LOG("Unhandled WM_STATE_TOGLE request %s", atom_name);
            XFree(atom_name);
        }
        return;
    }

    if (e.xclient.message_type == dsp.WM_CHANGE_STATE) {
        if (e.xclient.data.l[0] == IconicState) {
            set_minimized(true);
            XUnmapWindow(dsp, child);
            return;
        }
    }

    char* atom_name = XGetAtomName(dsp, e.xclient.message_type);
    TINY_LOG("Unhandled client message type %s", atom_name);
    XFree(atom_name);
}

void Window::on_motion_notify(const XEvent& e, void*)
{
    on_drag_motion(this, e);
}

void Window::on_focus_in(const XEvent& e, void*)
{
    on_focus(this, e, nullptr);
}

void Window::on_focus_out(const XEvent&, void*) {}

void Window::on_property_notify(const XEvent& e, void*)
{
    update_properties(); // the property could be new

    if (e.xproperty.atom == dsp.WM_PROTOCOLS) {
        update_protocols();
        return;
    }
    if (e.xproperty.atom == dsp._NET_WM_STATE) {
        update_wm_states();
        return;
    }
    if (e.xproperty.atom == dsp.WM_NORMAL_HINTS) {
        update_normal_hints();
        return;
    }
    if (e.xproperty.atom == dsp.WM_NAME && !is_net_wm_name) {
        char* data;
        if (XFetchName(dsp, child, &data)) {
            wm_name = data;
            free(data);
        }
        return;
    }
    if (e.xproperty.atom == dsp._NET_WM_NAME) {
        update_wm_name();
        return;
    }
    if (e.xproperty.atom == dsp.WM_ICON_NAME) {
        char* data;
        if (XGetIconName(dsp, child, &data)) {
            wm_icon_name = data;
            XFree(data);
        }
        return;
    }
    if (e.xproperty.atom == dsp._NET_WM_ICON_NAME) {
        update_wm_icon_name();
        return;
    }
    if (e.xproperty.atom == dsp._NET_WM_ICON) {
        update_wm_icon();
        return;
    }

    if (e.xproperty.atom == dsp._NET_WM_USER_TIME) {
        return; // changes with every key press, the focus policy ignores it
    }

    char* atom_name = XGetAtomName(dsp, e.xproperty.atom);
    TINY_LOG("Unhandled property update: %s", atom_name);
    XFree(atom_name);
}

void Window::on_key_release(const XEvent& e, void*)
{
    if (e.xkey.state & Mod1Mask &&
        e.xkey.keycode == XKeysymToKeycode(dsp, XK_F4)) {
        close();
    }
}

void Window::set_events()
{
    XSelectInput(dsp, child, PropertyChangeMask | FocusChangeMask);

    XGrabButton(dsp, Button1, AnyModifier, child, true,
                ButtonPressMask | ButtonReleaseMask, GrabModeSync, GrabModeSync,
                None, None);
    tiny::x_grab_key(dsp, XKeysymToKeycode(dsp, XK_F4), Mod1Mask, child);

    connect_window(
            FocusIn, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_focus_in));
    connect_window(
            FocusOut, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_focus_out));

    connect_window(
            ClientMessage, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_client_message));
    connect_window(PropertyNotify, child,
                   reinterpret_cast<tiny::event_signal_t>(
                           &Window::on_property_notify));
    connect_window(
            KeyRelease, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_key_release));
    connect_window(
            ButtonPress, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_button_press));
    connect_window(
            ButtonRelease, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_button_release));
    connect_window(
            MotionNotify, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_motion_notify));

    on_drag_begin.connect(reinterpret_cast<tiny::Object*>(this),
                          reinterpret_cast<tiny::object_signal_t>(
                                  &Window::on_window_drag_begin));
    on_drag_motion.connect(reinterpret_cast<tiny::Object*>(this),
                           reinterpret_cast<tiny::object_signal_t>(
                                   &Window::on_window_drag_motion));
}

int Window::get_properties(::Window window, std::set<Atom>& properties)
{
    int count;
    properties.clear();

    const tiny::Display& display = tiny::get_display();
    Atom* props = XListProperties(display, window, &count);

    for (int i = 0; i < count; ++i) {
        properties.insert(props[i]);
    }
    XFree(props);
    return count;
}

Window::WMType Window::get_net_wm_type(::Window window)
{
    WMType wm_type = WMType::NORMAL; // Default value
    const tiny::Display& display = tiny::get_display();

    Atom return_type;
    int size;
    unsigned long nitems;
    unsigned long bytes_left;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display, window, display._NET_WM_WINDOW_TYPE, 0L,
                           static_cast<long>(ULONG_MAX), false, XA_ATOM,
                           &return_type, &size, &nitems, &bytes_left,
                           &data) != Success) {
        // No _NET_WM_WINDOW_TYPE return
        return wm_type;
    }

    const Atom* window_types = reinterpret_cast<Atom*>(data);
    for (unsigned long i = 0; i < nitems; i++) {
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_DESKTOP) {
            wm_type = WMType::DESKTOP;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_DOCK) {
            wm_type = WMType::DOCK;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_TOOLBAR) {
            wm_type = WMType::TOOLBAR;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_MENU) {
            wm_type = WMType::MENU;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_UTILITY) {
            wm_type = WMType::UTILITY;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_SPLASH) {
            wm_type = WMType::SPLASH;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_DIALOG) {
            wm_type = WMType::DIALOG;
            break;
        }
        // NORMAL is default
    }
    XFree(data);
    return wm_type;
}

bool Window::get_motif_hints(::Window window, unsigned long& functions,
                             unsigned long& decorations)
{
    functions = MotifWMHints::FUNC_ALL | MotifWMHints::FUNC_RESIZE |
                MotifWMHints::FUNC_MOVE | MotifWMHints::FUNC_MINIMIZE |
                MotifWMHints::FUNC_MAXIMIZE | MotifWMHints::FUNC_CLOSE;

    decorations = MotifWMHints::DECOR_ALL | MotifWMHints::DECOR_BORDER |
                  MotifWMHints::DECOR_RESIZE | MotifWMHints::DECOR_TITLE |
                  MotifWMHints::DECOR_MENU | MotifWMHints::DECOR_MINIMIZE |
                  MotifWMHints::DECOR_MAXIMIZE;

    const tiny::Display& display = tiny::get_display();
    Atom returned_type;
    int size;
    unsigned long nitems;
    unsigned long bytes_left;
    MotifWMHints* mhints = nullptr;

    if (XGetWindowProperty(display, window, display._MOTIF_WM_HINTS, 0L,
                           MotifWMHints::property_length, false,
                           display._MOTIF_WM_HINTS, &returned_type, &size,
                           &nitems, &bytes_left,
                           reinterpret_cast<uint8_t**>(&mhints)) != Success ||
        !nitems) {
        return false;
    }
    if (mhints->flags & MotifWMHints::FLAG_FUNCTIONS) {
        functions = mhints->functions;
    }
    if (mhints->flags & MotifWMHints::FLAG_DECORATIONS) {
        decorations = mhints->decorations;
    }

    XFree(mhints);

    return true;
}

} // namespace wm
