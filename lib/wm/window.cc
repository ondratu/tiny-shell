#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cerrno>
#include <csignal>
#include <memory>
#include <new>

#include "wm/shell_theme.h"
#include "object.h"
#include "wm/process.h"
#include "wm/window.h"
#include "wm/work_area.h"
#include "image/pixmap.h"
#include "x_util.h"

namespace tiny::wm {

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

int fit_client_size(const XSizeHints& hints, bool horizontal, int size,
                    int floor)
{
    const Axis axis = axis_of(hints, horizontal);
    const auto on_grid = [&axis](int value) {
        return axis.inc > 0
                       ? axis.base + (value - axis.base) / axis.inc * axis.inc
                       : value;
    };

    if (axis.max > 0) {
        size = std::min(size, axis.max);
        // the limit of the client wins over the floor of the window manager
        floor = std::min(floor, on_grid(axis.max));
    }
    size = on_grid(size);

    const int lowest = std::max(axis.min, floor);
    if (size < lowest) {
        // the first size of the grid that is not under the minimum
        size = axis.inc > 0 ? size + (lowest - size + axis.inc - 1) / axis.inc *
                                              axis.inc
                            : lowest;
    }
    return size;
}

void fit_aspect(const XSizeHints& hints, int& width, int& height,
                bool keep_width)
{
    if (!(hints.flags & PAspect)) {
        return;
    }
    const Axis across = axis_of(hints, true);
    const Axis down = axis_of(hints, false);
    long long w = std::max(width - across.base, 0);
    long long h = std::max(height - down.base, 0);
    const long long max_x = hints.max_aspect.x;
    const long long max_y = hints.max_aspect.y;
    const long long min_x = hints.min_aspect.x;
    const long long min_y = hints.min_aspect.y;

    if (max_x > 0 && max_y > 0 && w * max_y > h * max_x) { // too wide
        if (keep_width) {
            h = (w * max_y + max_x - 1) / max_x;
        } else {
            w = h * max_x / max_y;
        }
    } else if (min_x > 0 && min_y > 0 && w * min_y < h * min_x) { // too tall
        if (keep_width) {
            h = w * min_y / min_x;
        } else {
            w = (h * min_x + min_y - 1) / min_y;
        }
    }
    // the limits of the client are stronger than its ratio
    const auto limited = [](const Axis& axis, long long size, int base) {
        int rv = static_cast<int>(size) + base;
        if (axis.max > 0) {
            rv = std::min(rv, axis.max);
        }
        return std::max(rv, axis.min);
    };
    width = limited(across, w, across.base);
    height = limited(down, h, down.base);
}

tiny::Rect resize_by_edges(const XSizeHints& hints, const tiny::Rect& start,
                           unsigned edges, int dx, int dy, int title,
                           int min_width, int min_height)
{
    tiny::Rect rv = start;
    if (edges & (tiny::Position::Left | tiny::Position::Right)) {
        const int width =
                start.width + (edges & tiny::Position::Right ? dx : -dx);
        rv.width = fit_client_size(hints, true, width, min_width);
    }
    if (edges & (tiny::Position::Top | tiny::Position::Bottom)) {
        const int height = start.height - title +
                           (edges & tiny::Position::Bottom ? dy : -dy);
        rv.height = fit_client_size(hints, false, height, min_height - title) +
                    title;
    }
    if (hints.flags & PAspect) {
        // the side that is dragged leads, the other one follows
        const bool across =
                edges & (tiny::Position::Left | tiny::Position::Right);
        const bool down =
                edges & (tiny::Position::Top | tiny::Position::Bottom);
        int client_height = rv.height - title;
        fit_aspect(hints, rv.width, client_height,
                   across && (!down || std::abs(dx) >= std::abs(dy)));
        // the grid and the minimum of the manager are stronger than the ratio
        rv.width = fit_client_size(hints, true, rv.width, min_width);
        client_height = fit_client_size(hints, false, client_height,
                                        min_height - title);
        rv.height = client_height + title;
    }
    if (edges & tiny::Position::Left) {
        rv.x = start.x + start.width - rv.width;
    }
    if (edges & tiny::Position::Top) {
        rv.y = start.y + start.height - rv.height;
    }
    return rv;
}

namespace {

// The reference point of a gravity in halves of the size: 0 is the west or
// north edge, 1 the middle, 2 the east or south edge.
void gravity_halves(int gravity, int& horizontal, int& vertical)
{
    horizontal = 0;
    vertical = 0;
    switch (gravity) {
        case NorthGravity:
        case CenterGravity:
        case SouthGravity:
            horizontal = 1;
            break;
        case NorthEastGravity:
        case EastGravity:
        case SouthEastGravity:
            horizontal = 2;
            break;
        default:
            break;
    }
    switch (gravity) {
        case WestGravity:
        case CenterGravity:
        case EastGravity:
            vertical = 1;
            break;
        case SouthWestGravity:
        case SouthGravity:
        case SouthEastGravity:
            vertical = 2;
            break;
        default:
            break;
    }
}

} // namespace

std::pair<int, int> frame_position(int gravity, int x, int y,
                                   const FrameExtents& extents)
{
    if (gravity == StaticGravity) {
        return {x - extents.left, y - extents.top};
    }
    int h;
    int v;
    gravity_halves(gravity, h, v);
    return {x - h * (extents.left + extents.right) / 2,
            y - v * (extents.top + extents.bottom) / 2};
}

std::pair<int, int> gravity_shift(int gravity, int dw, int dh)
{
    if (gravity == StaticGravity) {
        return {0, 0};
    }
    int h;
    int v;
    gravity_halves(gravity, h, v);
    return {-h * dw / 2, -v * dh / 2};
}

int window_gravity(const XSizeHints& hints)
{
    return (hints.flags & PWinGravity) ? hints.win_gravity : NorthWestGravity;
}

unsigned edges_of_direction(long direction)
{
    using tiny::Position;
    switch (direction) {
        case SIZE_TOPLEFT:
            return Position::Top | Position::Left;
        case SIZE_TOP:
            return Position::Top;
        case SIZE_TOPRIGHT:
            return Position::Top | Position::Right;
        case SIZE_RIGHT:
            return Position::Right;
        case SIZE_BOTTOMRIGHT:
            return Position::Bottom | Position::Right;
        case SIZE_BOTTOM:
            return Position::Bottom;
        case SIZE_BOTTOMLEFT:
            return Position::Bottom | Position::Left;
        case SIZE_LEFT:
            return Position::Left;
        default:
            return 0;
    }
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

static bool read_user_time_window(::Window window, ::Window& named);

// Names an atom in a debug message; in a normal build it asks the X server
// for nothing.
static void log_atom(::Display* display, const char* what, Atom atom)
{
#ifdef TINY_DEBUG
    char* name = XGetAtomName(display, atom);
    TINY_LOG("%s: %s", what, name ? name : "?");
    if (name) {
        XFree(name);
    }
#else
    (void)display;
    (void)what;
    (void)atom;
#endif
}

//! The bits of a Motif field that are allowed; `all` turns the bits around
static unsigned long motif_allowed(unsigned long value, unsigned long all,
                                   unsigned long everything)
{
    return (value & all) ? everything & ~value : value & everything;
}

void resolve_motif_hints(const unsigned long* data, unsigned long nitems,
                         unsigned long& functions, unsigned long& decorations)
{
    constexpr unsigned long all_functions =
            MotifWMHints::FUNC_RESIZE | MotifWMHints::FUNC_MOVE |
            MotifWMHints::FUNC_MINIMIZE | MotifWMHints::FUNC_MAXIMIZE |
            MotifWMHints::FUNC_CLOSE;
    constexpr unsigned long all_decorations =
            MotifWMHints::DECOR_BORDER | MotifWMHints::DECOR_RESIZE |
            MotifWMHints::DECOR_TITLE | MotifWMHints::DECOR_MENU |
            MotifWMHints::DECOR_MINIMIZE | MotifWMHints::DECOR_MAXIMIZE;

    functions = all_functions;
    decorations = all_decorations;
    if (data == nullptr || nitems < 1) {
        return;
    }
    // data[0] flags, data[1] functions, data[2] decorations
    if ((data[0] & MotifWMHints::FLAG_FUNCTIONS) && nitems > 1) {
        functions =
                motif_allowed(data[1], MotifWMHints::FUNC_ALL, all_functions);
    }
    if ((data[0] & MotifWMHints::FLAG_DECORATIONS) && nitems > 2) {
        decorations = motif_allowed(data[2], MotifWMHints::DECOR_ALL,
                                    all_decorations);
    }
}

XEvent make_pointer_event(int x_root, int y_root)
{
    XEvent e = {};
    e.type = ButtonPress;
    e.xbutton.x = x_root;
    e.xbutton.y = y_root;
    e.xbutton.x_root = x_root;
    e.xbutton.y_root = y_root;
    return e;
}

bool is_double_click(bool has_last, unsigned long last, unsigned long time)
{
    const unsigned long interval = 400; // ms
    return has_last &&
           static_cast<uint32_t>(time) - static_cast<uint32_t>(last) <=
                   interval;
}

bool takes_focus(bool has_time, unsigned long time, bool has_last,
                 unsigned long last)
{
    if (!has_time) {
        return true;
    }
    if (time == 0) {
        return false;
    }
    if (!has_last) {
        return true;
    }
    return static_cast<int32_t>(static_cast<uint32_t>(time - last)) >= 0;
}

Window::Window(::Window child_arg, ::Window root_arg,
               unsigned long functions_arg, const tiny::Rect& area):
    child(child_arg),
    root(root_arg),
    dsp(tiny::get_display()),
    work_area(area),
    functions(functions_arg)
{
    hints = XAllocSizeHints();
    if (hints == nullptr) {
        throw std::bad_alloc();
    }

    update_protocols();
    update_properties();
    update_transient_for();
    wm_type = get_net_wm_type(child_arg);

    if (properties.count(dsp._NET_WM_STATE)) {
        update_wm_states();
    }
    if (properties.count(dsp._NET_WM_DESKTOP)) {
        update_desktop();
    }
    if (properties.count(dsp.WM_NORMAL_HINTS)) {
        update_normal_hints();
    }

    init_restore_size(0); // a Client, which has a title bar, does it again

    update_wm_name();
    update_wm_class();

    if (properties.count(dsp._NET_WM_ICON)) {
        update_wm_icon();
    }
    if (not icon && properties.count(dsp.WM_HINTS)) {
        XIconSize* icon_size = XAllocIconSize();
        icon_size->min_width = shell_theme.icon;
        icon_size->min_height = shell_theme.icon;
        icon_size->max_width = shell_theme.icon;
        icon_size->max_height = shell_theme.icon;
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

void Window::init_restore_size(int title)
{
    if (!Window::get_maximized()) {
        return;
    }
    // two thirds of the work area, inside the limits and the ratio of the hints
    int width = work_area.width / 3 * 2;
    int height = work_area.height / 3 * 2 - title;
    if (hints->flags & PMinSize) {
        width = std::max(width, hints->min_width);
        height = std::max(height, hints->min_height);
    }
    if (hints->flags & PMaxSize) {
        width = std::min(width, hints->max_width);
        height = std::min(height, hints->max_height);
    }
    fit_aspect(*hints, width, height, true);
    state.width = width;
    state.height = height + title;
}

Window* Window::create(::Window root, ::Window child,
                       const XWindowAttributes& attrs, unsigned long functions,
                       const tiny::Rect& area)
{
    const tiny::Display& display = tiny::get_display();
    auto* win = new Window(child, root, functions, area);

    // inside of the work area, the border is outside of the window
    const tiny::Rect place = fit_in_area(
            {attrs.x, attrs.y, attrs.width, attrs.height}, inner_area(area, 1));
    XMoveResizeWindow(display, child, place.x, place.y,
                      tiny::x_dim(place.width), tiny::x_dim(place.height));
    XMapWindow(display, child);
    XSetWindowBorderWidth(display, child, 1);
    XRaiseWindow(display, child);

    win->set_wm_state(NormalState);
    win->publish_frame_extents();
    win->set_events();
    // TODO: move down under panel
    return win;
}

void Window::set_wm_state(long wm_state)
{
    const unsigned long data[2] = {static_cast<unsigned long>(wm_state),
                                   static_cast<unsigned long>(None)};
    XChangeProperty(dsp, child, dsp.WM_STATE, dsp.WM_STATE, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(data), 2);
}

void Window::withdraw()
{
    set_wm_state(WithdrawnState);
    XDeleteProperty(dsp, child, dsp._NET_WM_STATE);
    XDeleteProperty(dsp, child, dsp._NET_WM_DESKTOP);
    XDeleteProperty(dsp, child, dsp._NET_WM_ALLOWED_ACTIONS);
    XDeleteProperty(dsp, child, dsp._NET_FRAME_EXTENTS);
}

void Window::publish_frame_extents()
{
    const FrameExtents extents = frame_extents();
    const long values[] = {extents.left, extents.right, extents.top,
                           extents.bottom};
    XChangeProperty(dsp, child, dsp._NET_FRAME_EXTENTS, XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(values), 4);
}

void Window::fit_requested_size(unsigned long mask, int& width,
                                int& height) const
{
    if (mask & CWWidth) {
        width = fit_client_size(*hints, true, width);
    }
    if (mask & CWHeight) {
        height = fit_client_size(*hints, false, height);
    }
    if (mask & (CWWidth | CWHeight)) {
        // the side that was asked for is kept, the other one follows
        fit_aspect(*hints, width, height, mask & CWWidth);
    }
}

void Window::configure_request(const XConfigureRequestEvent& e)
{
    if (get_maximized() || is_fullscreen()) {
        send_configure_notify(); // the window keeps its place
        return;
    }

    XWindowChanges ch;
    ch.x = e.x;
    ch.y = e.y;
    ch.width = e.width;
    ch.height = e.height;
    ch.border_width = e.border_width;
    ch.sibling = e.above;
    ch.stack_mode = e.detail;
    auto mask = static_cast<unsigned int>(e.value_mask);

    XWindowAttributes now;
    if ((mask & (CWWidth | CWHeight)) &&
        XGetWindowAttributes(dsp, child, &now)) {
        ch.width = (mask & CWWidth) ? e.width : now.width;
        ch.height = (mask & CWHeight) ? e.height : now.height;
        fit_requested_size(mask, ch.width, ch.height);
        mask |= CWWidth | CWHeight;
        // A size change without a position keeps the reference point of the
        // gravity of the window where it was (ICCCM 4.1.2.3)
        if (!(mask & (CWX | CWY))) {
            const auto shift =
                    gravity_shift(window_gravity(*hints), ch.width - now.width,
                                  ch.height - now.height);
            ch.x = now.x + shift.first;
            ch.y = now.y + shift.second;
            mask |= CWX | CWY;
        }
    }

    XConfigureWindow(dsp, child, mask, &ch);
    send_configure_notify();
}

void Window::send_configure_notify()
{
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dsp, child, &attrs)) {
        return;
    }
    int x = 0;
    int y = 0;
    ::Window unused;
    if (!XTranslateCoordinates(dsp, child, root, 0, 0, &x, &y, &unused)) {
        return;
    }
    // the position of the outer corner, the border is a part of the window
    x -= attrs.border_width;
    y -= attrs.border_width;

    XConfigureEvent msg = {};
    msg.type = ConfigureNotify;
    msg.display = dsp;
    msg.event = child;
    msg.window = child;
    msg.x = x;
    msg.y = y;
    msg.width = attrs.width;
    msg.height = attrs.height;
    msg.border_width = attrs.border_width;
    msg.above = None;
    msg.override_redirect = False;
    XSendEvent(dsp, child, False, StructureNotifyMask,
               reinterpret_cast<XEvent*>(&msg));
}

void Window::set_minimized(bool minimized)
{
    if (minimized) {
        wm_states |= WMState::HIDDEN;
    } else {
        wm_states &= static_cast<unsigned long>(~WMState::HIDDEN);
    }

    set_wm_state(minimized ? IconicState : NormalState);
    publish_wm_states();
    on_minimize(nullptr, XEvent());
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
    publish_frame_extents();
}

void Window::set_fullscreen(bool fullscreen_arg)
{
    if (fullscreen_arg) {
        wm_states |= WMState::FULLSCREEN;
    } else {
        wm_states &= static_cast<unsigned long>(~WMState::FULLSCREEN);
    }

    publish_wm_states();
    publish_frame_extents();
}

//! Set or clear a flag of `_NET_WM_STATE`; true when it changed
static bool change_state(unsigned long& states, unsigned long flag, bool on)
{
    if (static_cast<bool>(states & flag) == on) {
        return false;
    }
    states = on ? states | flag : states & ~flag;
    return true;
}

void Window::set_shaded(bool shaded)
{
    if (change_state(wm_states, WMState::SHADED, shaded)) {
        publish_wm_states();
    }
}

void Window::set_skip_taskbar(bool skip)
{
    if (change_state(wm_states, WMState::SKIP_TASKBAR, skip)) {
        publish_wm_states();
        on_skip_taskbar(nullptr, XEvent());
    }
}

void Window::set_skip_pager(bool skip)
{
    if (change_state(wm_states, WMState::SKIP_PAGER, skip)) {
        publish_wm_states();
    }
}

void Window::set_sticky(bool sticky)
{
    if (change_state(wm_states, WMState::STICKY, sticky)) {
        publish_wm_states();
        publish_desktop();
    }
}

void Window::update_desktop()
{
    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dsp, child, dsp._NET_WM_DESKTOP, 0, 1, False,
                           XA_CARDINAL, &type, &format, &count, &left,
                           &data) == Success &&
        data) {
        // a window that asks for all the desktops is sticky
        // Xlib extends the 32 bit value to the long
        if (count == 1 && format == 32 &&
            (*reinterpret_cast<unsigned long*>(data) & all_desktops) ==
                    all_desktops) {
            wm_states |= WMState::STICKY;
        }
        XFree(data);
    }
}

std::vector<Atom> Window::get_allowed_actions() const
{
    std::vector<Atom> atoms;
    if (is_movable()) {
        atoms.push_back(dsp._NET_WM_ACTION_MOVE);
    }
    if (is_resizable()) {
        atoms.push_back(dsp._NET_WM_ACTION_RESIZE);
    }
    if (is_minimizable()) {
        atoms.push_back(dsp._NET_WM_ACTION_MINIMIZE);
    }
    if (is_shadable()) {
        atoms.push_back(dsp._NET_WM_ACTION_SHADE);
    }
    atoms.push_back(dsp._NET_WM_ACTION_STICK);
    if (is_maximizable()) {
        atoms.push_back(dsp._NET_WM_ACTION_MAXIMIZE_HORZ);
        atoms.push_back(dsp._NET_WM_ACTION_MAXIMIZE_VERT);
    }
    if (!is_auxiliary()) { // a dialog or a toolbar does not fill the screen
        atoms.push_back(dsp._NET_WM_ACTION_FULLSCREEN);
    }
    atoms.push_back(dsp._NET_WM_ACTION_CHANGE_DESKTOP);
    if (is_closable()) {
        atoms.push_back(dsp._NET_WM_ACTION_CLOSE);
    }
    atoms.push_back(dsp._NET_WM_ACTION_ABOVE);
    atoms.push_back(dsp._NET_WM_ACTION_BELOW);
    return atoms;
}

void Window::publish_allowed_actions()
{
    const std::vector<Atom> atoms = get_allowed_actions();
    XChangeProperty(dsp, child, dsp._NET_WM_ALLOWED_ACTIONS, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(atoms.data()),
                    static_cast<int>(atoms.size()));
}

void Window::publish_desktop()
{
    const long desktop = is_sticky() ? static_cast<long>(all_desktops) : 0;
    XChangeProperty(dsp, child, dsp._NET_WM_DESKTOP, XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&desktop), 1);
}

void Window::set_above(bool above)
{
    // a window is in one layer
    const bool changed = change_state(wm_states, WMState::ABOVE, above);
    if (above) {
        change_state(wm_states, WMState::BELOW, false);
    }
    if (changed) {
        publish_wm_states();
        on_layer(nullptr, XEvent());
    }
}

void Window::set_below(bool below)
{
    const bool changed = change_state(wm_states, WMState::BELOW, below);
    if (below) {
        change_state(wm_states, WMState::ABOVE, false);
    }
    if (changed) {
        publish_wm_states();
        on_layer(nullptr, XEvent());
    }
}

void Window::set_demands_attention(bool demands)
{
    if (demands && (wm_states & WMState::FOCUSED)) {
        return; // the user looks at the window already
    }
    if (change_state(wm_states, WMState::DEMANDS_ATTENTION, demands)) {
        publish_wm_states();
        on_attention(nullptr, XEvent());
    }
}

void Window::set_modal(bool modal)
{
    if (modal) {
        wm_states |= WMState::MODAL;
    } else {
        wm_states &= static_cast<unsigned long>(~WMState::MODAL);
    }
    publish_wm_states();
    on_modal(nullptr, XEvent());
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
    // the user looks at the window now
    wm_states &= static_cast<unsigned long>(~WMState::DEMANDS_ATTENTION);

    publish_wm_states();
}

void Window::raise()
{
    XRaiseWindow(dsp, child);
}

void Window::lower()
{
    XLowerWindow(dsp, child);
}

void Window::stack_under(::Window sibling)
{
    XWindowChanges changes = {};
    changes.sibling = sibling;
    changes.stack_mode = Below;
    XConfigureWindow(dsp, child, CWSibling | CWStackMode, &changes);
}

void Window::move_to(int x, int y)
{
    XMoveWindow(dsp, child, x, y);
    send_configure_notify();
}

void Window::return_focus()
{
    wm_states &= static_cast<unsigned long>(~WMState::FOCUSED);

    publish_wm_states();
}

bool Window::ask_to_close()
{
    if (!protocols.count(dsp.WM_DELETE_WINDOW)) {
        return false;
    }
    XClientMessageEvent msg = {};
    msg.window = child;
    msg.type = ClientMessage;
    msg.format = 32;
    msg.message_type = dsp.WM_PROTOCOLS;
    msg.data.l[0] = static_cast<long>(dsp.WM_DELETE_WINDOW);
    XSendEvent(dsp, child, false, NoEventMask, reinterpret_cast<XEvent*>(&msg));
    return true;
}

void Window::close()
{
    if (!responding) {
        // the user decides, the next ping may even bring the client back
        ask_at = Clock::now();
        on_unresponsive(nullptr, XEvent());
        return;
    }
    if (!ask_to_close()) {
        XKillClient(dsp, child);
        return;
    }
    ping(Clock::now()); // find out whether it answers
}

pid_t Window::get_killable_pid() const
{
    Atom type;
    int format;
    unsigned long nitems;
    unsigned long leftover;
    unsigned char* data = nullptr;
    unsigned long pid = 0;
    if (XGetWindowProperty(dsp, child, dsp._NET_WM_PID, 0, 1, False,
                           XA_CARDINAL, &type, &format, &nitems, &leftover,
                           &data) == Success &&
        data) {
        if (type == XA_CARDINAL && format == 32 && nitems == 1) {
            pid = *reinterpret_cast<unsigned long*>(data);
        }
        XFree(data);
    }
    std::string machine;
    XTextProperty text = {};
    if (XGetWMClientMachine(dsp, child, &text) && text.value) {
        machine = reinterpret_cast<char*>(text.value);
        XFree(text.value);
    }
    return killable_pid(pid, machine, local_host_name());
}

bool Window::client_process_gone() const
{
    const pid_t pid = get_killable_pid();
    return pid && kill(pid, 0) < 0 && errno == ESRCH;
}

void Window::kill_client()
{
    const pid_t pid = get_killable_pid();
    if (pid) {
        kill(pid, SIGKILL);
    }
    XKillClient(dsp, child);
}

void Window::ping(Clock::time_point now)
{
    if (ping_pending || !protocols.count(dsp._NET_WM_PING)) {
        return;
    }
    XClientMessageEvent msg = {};
    msg.window = child;
    msg.type = ClientMessage;
    msg.format = 32;
    msg.message_type = dsp.WM_PROTOCOLS;
    msg.data.l[0] = static_cast<long>(dsp._NET_WM_PING);
    msg.data.l[1] = CurrentTime;
    msg.data.l[2] = static_cast<long>(child);
    XSendEvent(dsp, child, false, NoEventMask, reinterpret_cast<XEvent*>(&msg));
    ping_pending = true;
    ping_deadline = now + ping_timeout;
    next_ping = now + ping_interval;
}

void Window::pong()
{
    ping_pending = false;
    if (!responding) {
        responding = true;
        on_responding_changed();
    }
}

bool Window::expire_ping(Clock::time_point now)
{
    if (!ping_pending || now < ping_deadline) {
        return false;
    }
    ping_pending = false;
    if (responding) {
        responding = false;
        ask_at = now + ask_delay;
        on_responding_changed();
    }
    return true;
}

void Window::ping_if_due(Clock::time_point now)
{
    if (!get_minimized() && now >= next_ping) {
        ping(now);
    }
}

std::optional<Window::Clock::time_point> Window::get_ping_wakeup() const
{
    if (ping_pending) {
        return ping_deadline;
    }
    if (!protocols.count(dsp._NET_WM_PING) || get_minimized()) {
        return std::nullopt;
    }
    return next_ping;
}

std::optional<Window::Clock::time_point> Window::get_ask_time() const
{
    if (responding || get_minimized()) {
        return std::nullopt; // nobody waits for a window that is not shown
    }
    return ask_at;
}

void Window::postpone_ask(Clock::time_point now)
{
    ask_at = now + wait_delay;
}

void Window::minimize()
{
    if (get_minimized()) {
        return;
    }

    // without a frame there is only the client to hide, and it is told so
    XWindowAttributes attrs;
    if (XGetWindowAttributes(dsp, child, &attrs) &&
        attrs.map_state != IsUnmapped) {
        ++expected_unmaps;
        XUnmapWindow(dsp, child);
    }
    if (is_focused()) {
        XSetInputFocus(dsp, PointerRoot, RevertToPointerRoot, CurrentTime);
    }
    set_minimized(true);
}

bool Window::consume_unmap()
{
    if (!expected_unmaps) {
        return false;
    }
    --expected_unmaps;
    return true;
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

    place_maximized();
    set_maximized(true);
}

void Window::place_maximized()
{
    XMoveResizeWindow(dsp, child, work_area.x, work_area.y,
                      tiny::x_uint(work_area.width),
                      tiny::x_uint(work_area.height));
}

void Window::set_work_area(const tiny::Rect& area)
{
    work_area = area;
    if (is_fullscreen()) {
        return;
    }
    if (get_maximized()) {
        place_maximized();
    } else {
        keep_in_work_area();
    }
}

void Window::keep_in_work_area()
{
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dsp, child, &attrs)) {
        return;
    }
    // the border is outside of the window
    const tiny::Rect place =
            fit_in_area({attrs.x, attrs.y, attrs.width, attrs.height},
                        inner_area(work_area, 1));
    if (place.x != attrs.x || place.y != attrs.y ||
        place.width != attrs.width || place.height != attrs.height) {
        XMoveResizeWindow(dsp, child, place.x, place.y,
                          tiny::x_dim(place.width), tiny::x_dim(place.height));
    }
}

void Window::fullscreen(bool on)
{
    if (on == is_fullscreen()) {
        return;
    }

    if (on) {
        XWindowAttributes attrs;
        XGetWindowAttributes(dsp, child, &attrs);
        before_fullscreen = {attrs.x, attrs.y, attrs.width, attrs.height};
        XGetWindowAttributes(dsp, root, &attrs);
        XMoveResizeWindow(dsp, child, 0, 0, tiny::x_uint(attrs.width),
                          tiny::x_uint(attrs.height));
        XRaiseWindow(dsp, child);
    } else if (get_maximized()) {
        set_fullscreen(false);
        place_maximized(); // the work area can have changed meanwhile
        return;
    } else {
        XMoveResizeWindow(dsp, child, before_fullscreen.x, before_fullscreen.y,
                          tiny::x_uint(before_fullscreen.width),
                          tiny::x_uint(before_fullscreen.height));
    }
    set_fullscreen(on);
}

void Window::apply_initial_state()
{
    // the flags were read from the client, the geometry is not there yet
    const bool maximized = get_maximized();
    const bool fullscreen_state = is_fullscreen();
    if (maximized) {
        // the window is not on screen in any other size: what it returns to is
        // the size chosen in the constructor, not the size it is created with
        const tiny::Rect size = state;
        set_maximized(false);
        maximize();
        if (size.width > 0 && size.height > 0) {
            state.width = size.width; // restore() puts it inside the area
            state.height = size.height;
        }
    }
    if (fullscreen_state) {
        set_fullscreen(false);
        fullscreen(true);
    }
    if (is_shaded()) {
        set_shaded(false); // the frame is not rolled up yet
        shade(true);
    }
}

void Window::restore(int x, int y)
{
    if (get_maximized()) {
        // the window returns inside of the work area, which may have changed
        const tiny::Rect place =
                fit_in_area({x ? (x - state.width / 2) : state.x,
                             y ? (y - state.height / 2) : state.y, state.width,
                             state.height},
                            inner_area(work_area, 1));
        state.width = place.width;
        state.height = place.height;

        XMoveResizeWindow(dsp, child, place.x, place.y,
                          tiny::x_uint(place.width),
                          tiny::x_uint(place.height));
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

void Window::update_transient_for()
{
    if (!XGetTransientForHint(dsp, child, &transient_for)) {
        transient_for = None;
    }
}

void Window::update_normal_hints()
{
    /*
     * USPosition|USSize|PPosition|PSize|PMinSize|PMaxSize|PResizeInc|PAspect
     * PBaseSize|PWinGravity
     */
    long supplied; // fields Xlib understands, the client's are in hints->flags
    if (!XGetWMNormalHints(dsp, child, hints, &supplied)) {
        hints->flags = 0; // no hints: no limits
    }
    // No limits means no restriction (ICCCM); only equal limits fix the size
    if (hints->flags & PMinSize && hints->flags & PMaxSize &&
        (hints->min_width == hints->max_width) &&
        (hints->min_height == hints->max_height)) {
        functions &= static_cast<unsigned long>(~MotifWMHints::FUNC_RESIZE);
        functions &= static_cast<unsigned long>(~MotifWMHints::FUNC_MAXIMIZE);
    }
}

void Window::update_wm_hints()
{
    XWMHints* wm_hints = XGetWMHints(dsp, child);
    window_group = None;
    if (wm_hints == nullptr) {
        return;
    }
    if (wm_hints->flags & WindowGroupHint) {
        window_group = wm_hints->window_group;
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

    if (iw == shell_theme.icon || ih == shell_theme.icon) {
        icon = XCreatePixmap(dsp, geo_root, tiny::x_uint(shell_theme.icon),
                             tiny::x_uint(shell_theme.icon),
                             tiny::x_uint(XDefaultDepth(
                                     dsp, 0))); // TODO: Screen instead of 0

        GC gc = XCreateGC(dsp, icon, 0, nullptr);
        XCopyArea(dsp, wm_hints->icon_pixmap, icon, gc, ib + wm_hints->icon_x,
                  ib + wm_hints->icon_y, tiny::x_uint(shell_theme.icon),
                  tiny::x_uint(shell_theme.icon), shell_theme.icon / 2 - iw / 2,
                  shell_theme.icon / 2 - ih / 2);
        XFreeGC(dsp, gc);

        if (wm_hints->flags & IconMaskHint) {
            icon_mask =
                    XCreatePixmap(dsp, geo_root, tiny::x_uint(shell_theme.icon),
                                  tiny::x_uint(shell_theme.icon), 1);

            GC mask_gc = XCreateGC(dsp, icon_mask, 0, nullptr);
            XCopyArea(dsp, wm_hints->icon_mask, icon_mask, mask_gc,
                      ib + wm_hints->icon_x, ib + wm_hints->icon_y,
                      tiny::x_uint(shell_theme.icon),
                      tiny::x_uint(shell_theme.icon),
                      shell_theme.icon / 2 - iw / 2,
                      shell_theme.icon / 2 - ih / 2);
            XFreeGC(dsp, mask_gc);
        }

    } else {
        icon = image::resize_pixmap(
                dsp, geo_root, wm_hints->icon_pixmap, icon_width, icon_height,
                tiny::x_uint(shell_theme.icon), tiny::x_uint(shell_theme.icon),
                tiny::x_uint(XDefaultDepth(dsp, 0)));
        if (wm_hints->flags & IconMaskHint) {
            icon_mask = image::resize_pixmap(dsp, geo_root, wm_hints->icon_mask,
                                             icon_width, icon_height,
                                             tiny::x_uint(shell_theme.icon),
                                             tiny::x_uint(shell_theme.icon), 1);
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

        log_atom(dsp, "Unsupported _NET_WM_STATE", atoms[i]);
    }
    XFree(data);
}

void Window::update_wm_class()
{
    XClassHint class_hint = {};
    if (!XGetClassHint(dsp, child, &class_hint)) {
        return;
    }
    wm_class[0] = class_hint.res_name != nullptr ? class_hint.res_name : "";
    wm_class[1] = class_hint.res_class != nullptr ? class_hint.res_class : "";
    XFree(class_hint.res_name);
    XFree(class_hint.res_class);
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
        unsigned long width = 0;
        unsigned long height = 0;
        // Yes, it can really be 64bit (unsigned long) not 32bit!
        auto* words = reinterpret_cast<unsigned long*>(data);
        while (pos < nitems) {
            width = words[pos];
            height = words[pos + 1];
            icons[width] = pos;
            pos += 2 + width * height; // width+height+pixels
        }

        for (auto it : icons) { // Find the best icon size
            pos = it.second;
            if (it.first >= static_cast<unsigned long>(shell_theme.icon)) {
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
        // format is RGBA + 4 zeros on 64bit
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                const auto* pixel = reinterpret_cast<const uint8_t*>(
                        &words[pos + y * width + x]);
                uint32_t color =
                        static_cast<uint32_t>(pixel[0] * pixel[3] / 255) << 0;
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

        const auto icon_px = static_cast<unsigned long>(shell_theme.icon);
        if ((width != icon_px) && (height != icon_px)) {
            Pixmap orig = icon;
            icon = image::resize_pixmap(dsp, root, orig,
                                        static_cast<unsigned>(width),
                                        static_cast<unsigned>(height),
                                        tiny::x_uint(shell_theme.icon),
                                        tiny::x_uint(shell_theme.icon),
                                        tiny::x_uint(XDefaultDepth(dsp, 0)));
            XFreePixmap(dsp, orig);

            orig = icon_mask;
            icon_mask = image::resize_pixmap(dsp, root, orig,
                                             static_cast<unsigned>(width),
                                             static_cast<unsigned>(height),
                                             tiny::x_uint(shell_theme.icon),
                                             tiny::x_uint(shell_theme.icon), 1);
            XFreePixmap(dsp, orig);
        }

        XFreeGC(dsp, gc);
        XFreeGC(dsp, gc_mask);
        XFree(data);
    }
}

void Window::begin_drag(unsigned edges, int x_root, int y_root)
{
    drag_edges = edges;
    start_event = make_pointer_event(x_root, y_root);
    XGetWindowAttributes(dsp, child, &start_attrs);
}

void Window::drag_to(int x_root, int y_root)
{
    const int dx = x_root - start_event.xbutton.x_root;
    const int dy = y_root - start_event.xbutton.y_root;
    if (!drag_edges) {
        XMoveWindow(dsp, child, start_attrs.x + dx, start_attrs.y + dy);
        return;
    }
    const tiny::Rect r =
            resize_by_edges(*hints,
                            {start_attrs.x, start_attrs.y, start_attrs.width,
                             start_attrs.height},
                            drag_edges, dx, dy, 0, 1, 1);
    XMoveResizeWindow(dsp, child, r.x, r.y, tiny::x_dim(r.width),
                      tiny::x_dim(r.height));
}

void Window::end_drag()
{
    drag_edges = 0;
    send_configure_notify();
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

    if (e.xbutton.state & (Mod1Mask | Mod4Mask)) { // Alt or Super
        on_modifier_drag(nullptr, e);
        // unblock the pointer, but not send to child
        XAllowEvents(dsp, AsyncPointer, e.xbutton.time);
        XGrabPointer(dsp, child, false,
                     ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
                     GrabModeAsync, GrabModeAsync, None,
                     XCreateFontCursor(dsp, XC_hand1), CurrentTime);
        on_drag_begin(nullptr, e);
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

void Window::on_net_wm_state(const XClientMessageEvent& e)
{
    const long action = e.data.l[0];
    if (action != tiny::Display::_NET_WM_STATE_REMOVE &&
        action != tiny::Display::_NET_WM_STATE_ADD &&
        action != tiny::Display::_NET_WM_STATE_TOGGLE) {
        return; // not a request
    }
    bool resize_maximized = false;
    const auto requested = [action](bool current) {
        return action == tiny::Display::_NET_WM_STATE_TOGGLE
                       ? !current
                       : action == tiny::Display::_NET_WM_STATE_ADD;
    };
    // the two atoms of a request are independent (EWMH 1.5)
    for (const int i : {1, 2}) {
        const auto atom = static_cast<Atom>(e.data.l[i]);
        if (atom == None) {
            continue;
        }
        if (atom == dsp._NET_WM_STATE_FULLSCREEN) {
            fullscreen(action == tiny::Display::_NET_WM_STATE_TOGGLE
                               ? !is_fullscreen()
                               : action == tiny::Display::_NET_WM_STATE_ADD);
        } else if (atom == dsp._NET_WM_STATE_SHADED) {
            shade(requested(is_shaded()));
        } else if (atom == dsp._NET_WM_STATE_SKIP_TASKBAR) {
            set_skip_taskbar(requested(skip_taskbar()));
        } else if (atom == dsp._NET_WM_STATE_SKIP_PAGER) {
            set_skip_pager(requested(skip_pager()));
        } else if (atom == dsp._NET_WM_STATE_STICKY) {
            set_sticky(requested(is_sticky()));
        } else if (atom == dsp._NET_WM_STATE_ABOVE) {
            set_above(requested(is_above()));
        } else if (atom == dsp._NET_WM_STATE_BELOW) {
            set_below(requested(is_below()));
        } else if (atom == dsp._NET_WM_STATE_DEMANDS_ATTENTION) {
            set_demands_attention(requested(demands_attention()));
        } else if (atom == dsp._NET_WM_STATE_MODAL) {
            set_modal(action == tiny::Display::_NET_WM_STATE_TOGGLE
                              ? !is_modal()
                              : action == tiny::Display::_NET_WM_STATE_ADD);
        } else if (atom == dsp._NET_WM_STATE_MAXIMIZED_VERT ||
                   atom == dsp._NET_WM_STATE_MAXIMIZED_HORZ) {
            resize_maximized = true; // only both directions at once
        } else {
            log_atom(dsp, "Unhandled _NET_WM_STATE request", atom);
        }
    }
    if (!resize_maximized) {
        return;
    }
    const bool on = action == tiny::Display::_NET_WM_STATE_TOGGLE
                            ? !get_maximized()
                            : action == tiny::Display::_NET_WM_STATE_ADD;
    if (on) {
        maximize();
    } else if (get_maximized()) {
        restore();
    }
}

void Window::on_client_message(const XEvent& e, void*)
{
    if (e.xclient.message_type == dsp._NET_WM_STATE) {
        on_net_wm_state(e.xclient);
        return;
    }

    if (e.xclient.message_type == dsp._NET_WM_MOVERESIZE) {
        on_move_resize_request(nullptr, e);
        return;
    }

    if (e.xclient.message_type == dsp.WM_CHANGE_STATE) {
        if (e.xclient.data.l[0] == IconicState) {
            if (is_minimizable()) {
                minimize(); // the frame goes with it
            }
            return;
        }
    }

    log_atom(dsp, "Unhandled client message type", e.xclient.message_type);
}

void Window::on_motion_notify(const XEvent& e, void*)
{
    on_drag_motion(nullptr, e);
}

bool Window::is_focus_change(const XEvent& e)
{
    if (e.xfocus.mode == NotifyGrab || e.xfocus.mode == NotifyUngrab) {
        return false;
    }
    return e.xfocus.detail != NotifyInferior &&
           e.xfocus.detail != NotifyPointer &&
           e.xfocus.detail != NotifyPointerRoot &&
           e.xfocus.detail != NotifyDetailNone;
}

void Window::on_focus_in(const XEvent& e, void*)
{
    if (!is_focus_change(e)) {
        return;
    }
    on_focus(nullptr, e, nullptr);
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
    if (e.xproperty.atom == dsp._NET_WM_DESKTOP) {
        return; // ours: the manager decides, a pager asks with a message
    }
    if (e.xproperty.atom == dsp.WM_NORMAL_HINTS) {
        update_normal_hints();
        publish_allowed_actions(); // a fixed size cannot be resized
        return;
    }
    if (e.xproperty.atom == dsp._MOTIF_WM_HINTS) {
        unsigned long new_decorations;
        // the shadow of the frame exists (or not) from the start
        const unsigned long resize = functions & MotifWMHints::FUNC_RESIZE;
        get_motif_hints(child, functions, new_decorations); // none: all
        functions &= static_cast<unsigned long>(~MotifWMHints::FUNC_RESIZE);
        functions |= resize;
        update_normal_hints(); // the size hints limit the functions too
        on_motif_hints_changed(new_decorations);
        publish_allowed_actions();
        return;
    }
    if (e.xproperty.atom == XA_WM_TRANSIENT_FOR) {
        update_transient_for();
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

    if (e.xproperty.atom == dsp.WM_STATE ||
        e.xproperty.atom == dsp._NET_FRAME_EXTENTS) {
        return; // written by the window manager itself
    }

    if (e.xproperty.atom == dsp._NET_WM_USER_TIME) {
        on_user_time(nullptr, XEvent()); // with every key press
        return;
    }

    if (e.xproperty.atom == dsp._NET_WM_USER_TIME_WINDOW) {
        watch_user_time_window();
        on_user_time(nullptr, XEvent());
        return;
    }

    log_atom(dsp, "Unhandled property update", e.xproperty.atom);
}

void Window::on_motif_hints_changed(unsigned long new_decorations)
{
    if (new_decorations) {
        on_reframe(nullptr, XEvent()); // it is a window without a frame now
    }
}

void Window::on_user_time_window_notify(const XEvent& e, void*)
{
    if (e.xproperty.atom == dsp._NET_WM_USER_TIME) {
        on_user_time(nullptr, XEvent());
    }
}

void Window::watch_user_time_window()
{
    ::Window named = None;
    if (!read_user_time_window(child, named)) {
        named = None;
    }
    if (named == child) {
        named = None;
    }
    if (named == user_time_window) {
        return;
    }
    if (user_time_window != None) {
        window_events.disconnect(PropertyNotify, user_time_window);
    }
    user_time_window = named;
    if (named != None) {
        XWindowAttributes attrs;
        long mask = PropertyChangeMask;
        if (XGetWindowAttributes(dsp, named, &attrs)) {
            mask |= attrs.your_event_mask; // the window may be ours already
        }
        XSelectInput(dsp, named, mask);
        window_events.connect(PropertyNotify, named, [this](const XEvent& e) {
            on_user_time_window_notify(e, nullptr);
        });
    }
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

    watch_user_time_window();

    window_events.connect(FocusIn, child,
                          [this](const XEvent& e) { on_focus_in(e, nullptr); });
    window_events.connect(FocusOut, child, [this](const XEvent& e) {
        on_focus_out(e, nullptr);
    });

    window_events.connect(ClientMessage, child, [this](const XEvent& e) {
        on_client_message(e, nullptr);
    });
    window_events.connect(PropertyNotify, child, [this](const XEvent& e) {
        on_property_notify(e, nullptr);
    });
    window_events.connect(KeyRelease, child, [this](const XEvent& e) {
        on_key_release(e, nullptr);
    });
    window_events.connect(ButtonPress, child, [this](const XEvent& e) {
        on_button_press(e, nullptr);
    });
    window_events.connect(ButtonRelease, child, [this](const XEvent& e) {
        on_button_release(e, nullptr);
    });
    window_events.connect(MotionNotify, child, [this](const XEvent& e) {
        on_motion_notify(e, nullptr);
    });

    on_drag_begin.connect([this](tiny::Object* o, const XEvent& e, void* d) {
        on_window_drag_begin(o, e, d);
    });
    on_drag_motion.connect([this](tiny::Object* o, const XEvent& e, void* d) {
        on_window_drag_motion(o, e, d);
    });
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

static bool read_user_time_window(::Window window, ::Window& named)
{
    const tiny::Display& display = tiny::get_display();
    Atom returned_type;
    int size;
    unsigned long nitems = 0;
    unsigned long bytes_left;
    unsigned char* data = nullptr;

    const bool found =
            XGetWindowProperty(display, window,
                               display._NET_WM_USER_TIME_WINDOW, 0L, 1L, false,
                               XA_WINDOW, &returned_type, &size, &nitems,
                               &bytes_left, &data) == Success &&
            data && nitems == 1 && size == 32 && returned_type == XA_WINDOW;
    if (found) {
        named = *reinterpret_cast<::Window*>(data);
    }
    if (data) {
        XFree(data);
    }
    return found;
}

bool Window::get_user_time(::Window window, unsigned long& time)
{
    const tiny::Display& display = tiny::get_display();
    ::Window named;
    if (read_user_time_window(window, named) && named != None) {
        window = named; // the time is kept on a window of its own
    }
    Atom returned_type;
    int size;
    unsigned long nitems = 0;
    unsigned long bytes_left;
    unsigned char* data = nullptr;

    const bool found =
            XGetWindowProperty(display, window, display._NET_WM_USER_TIME, 0L,
                               1L, false, XA_CARDINAL, &returned_type, &size,
                               &nitems, &bytes_left, &data) == Success &&
            data && nitems == 1 && size == 32 && returned_type == XA_CARDINAL;
    if (found) {
        time = *reinterpret_cast<unsigned long*>(data);
    }
    if (data) {
        XFree(data);
    }
    return found;
}

bool Window::get_motif_hints(::Window window, unsigned long& functions,
                             unsigned long& decorations)
{
    const tiny::Display& display = tiny::get_display();
    Atom returned_type;
    int size;
    unsigned long nitems = 0;
    unsigned long bytes_left;
    unsigned char* data = nullptr;

    const bool found =
            XGetWindowProperty(display, window, display._MOTIF_WM_HINTS, 0L,
                               MotifWMHints::property_length, false,
                               display._MOTIF_WM_HINTS, &returned_type, &size,
                               &nitems, &bytes_left, &data) == Success &&
            data && nitems && size == 32 &&
            returned_type == display._MOTIF_WM_HINTS;
    resolve_motif_hints(found ? reinterpret_cast<const unsigned long*>(data)
                              : nullptr,
                        found ? nitems : 0, functions, decorations);
    if (data) {
        XFree(data);
    }
    return found;
}

} // namespace tiny::wm
