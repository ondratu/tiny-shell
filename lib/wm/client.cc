#include <X11/cursorfont.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>

#include "object.h"
#include "theme.h"
#include "wm/shell_theme.h"
#include "wm/window.h"
#include "wm/client.h"
#include "wm/work_area.h"
#include "x_util.h"

namespace tiny {
extern std::shared_ptr<Display> display;
}

namespace tiny::wm {

static bool has_title(unsigned long decorations)
{
    return decorations & MotifWMHints::DECOR_TITLE;
}

static int header_height_for(unsigned long decorations)
{
    return has_title(decorations) ? shell_theme.win_header : 0;
}

static int border_for(unsigned long decorations)
{
    return (decorations & MotifWMHints::DECOR_BORDER)
                   ? tiny::theme.widget.border_width
                   : 0;
}

Client::Client(::Window child_arg, ::Window root_arg, int w, int h,
               unsigned long functions_arg, unsigned long decorations_arg,
               const tiny::Rect& area):
    tiny::Container(w, h + header_height_for(decorations_arg), "wm_window"),
    Window(child_arg, root_arg, functions_arg, area),
    decorations(decorations_arg),
    header(w, shell_theme.win_header),
    shadow(w, h + header_height_for(decorations_arg))
{
    state = {0, 0, 0, 0}; // the size of the base class had no title bar
    init_restore_size(header_height_for(decorations_arg));
}

static int ignore_x_error(::Display*, XErrorEvent*)
{
    return 0;
}

Client::~Client()
{
    // Give the client back to the root, so it survives the frame (it can be
    // gone already, e.g. when it was closed).
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs)) {
        XErrorHandler old = XSetErrorHandler(ignore_x_error);
        XReparentWindow(display, child, parent, attrs.x,
                        attrs.y + header_height());
        XSync(display, False);
        XSetErrorHandler(old);
    }

    window_events.disconnect(PropertyNotify, child);

    XUngrabButton(display, Button1, AnyModifier, window);
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_F4), Mod1Mask,
                       window);

    disconnect(ButtonPress);
    disconnect(ButtonRelease);
    disconnect(MotionNotify);
    disconnect(FocusIn);
    disconnect(FocusOut);
}

//! A new window is moved into the work area, a big one is made smaller
static tiny::Rect first_place(const XWindowAttributes& attrs,
                              const XSizeHints& hints, const tiny::Rect& area,
                              unsigned long decorations)
{
    const int border = border_for(decorations); // on both sides
    const int header = header_height_for(decorations);
    int x = attrs.x;
    int y = attrs.y;
    // A position that the user or the program chose is the place of the
    // window as if it had no frame; the gravity tells which point of the frame
    // goes there. Any other position is not a wish.
    if (hints.flags & (USPosition | PPosition)) {
        std::tie(x, y) = frame_position(window_gravity(hints), x, y,
                                        Client::frame_extents_for(decorations));
    }
    // the border is drawn outside of the window, so it is not over the panel
    return fit_in_area({x, y, attrs.width, attrs.height + header},
                       inner_area(area, border));
}

Window* Client::create(::Window root, ::Window child,
                       const XWindowAttributes& attrs, unsigned long functions,
                       unsigned long decorations, const tiny::Rect& area,
                       bool fresh)
{
    XSizeHints hints = {};
    long supplied;
    if (!fresh ||
        !XGetWMNormalHints(tiny::get_display(), child, &hints, &supplied)) {
        hints.flags = 0; // the position of a window that is there is real
    }
    const tiny::Rect place = first_place(attrs, hints, area, decorations);
    const int client_height = place.height - header_height_for(decorations);
    auto* win = new Client(child, root, place.width, client_height, functions,
                           decorations, area);
    if (place.width != attrs.width || client_height != attrs.height) {
        XResizeWindow(win->dsp, child, tiny::x_dim(place.width),
                      tiny::x_dim(client_height));
    }
    win->realize(root, place.x, place.y);
    win->on_focus_out(XEvent(), nullptr); // disable and GrabButton
    win->map_all();

    win->set_wm_state(NormalState);
    win->publish_frame_extents();
    return win;
}

void Client::realize(::Window parent_arg, int x, int y)
{
    update_properties();
    tiny::Container::realize(parent_arg, x, y);

    if (is_resizable()) {
        shadow.realize(parent_arg, x, y);
    }

    add(&header, 0, 0);
    // all of them exist, the hints can change; layout_buttons() shows some
    for (tiny::Widget* button : {static_cast<tiny::Widget*>(&cls_btn),
                                 static_cast<tiny::Widget*>(&max_btn),
                                 static_cast<tiny::Widget*>(&min_btn)}) {
        header.add(button, 0, 0, SouthEastGravity);
    }
    layout_buttons();

    XSetWindowBorderWidth(display, child, 0);
    XReparentWindow(display, child, window, 0, header_height());
    header.set_title(title_text());
}

void Client::set_events(long mask)
{
    // SubstructureNotifyMask so, tiny::wm::Manager can catch UnmapNotify
    // SubstructureRedirectMask so the requests of the client to move or
    // resize itself come to configure_request()
    tiny::Container::set_events(mask | SubstructureNotifyMask |
                                SubstructureRedirectMask | FocusChangeMask);
    Window::set_events();

    /*
    XSelectInput(display, window,
        SubstructureRedirectMask | SubstructureNotifyMask | FocusChangeMask);
*/

    header.get_title_box()->on_drag_begin.connect(
            [this](tiny::Object* o, const XEvent& e, void* d) {
                on_window_drag_begin(o, e, d);
            });
    header.get_title_box()->on_drag_motion.connect(
            [this](tiny::Object* o, const XEvent& e, void* d) {
                on_window_drag_motion(o, e, d);
            });

    header.get_title_box()->on_drag_end.connect(
            [this](tiny::Object* o, const XEvent& e, void* d) {
                on_window_drag_end(o, e, d);
            });

    min_btn.on_click.connect([this](tiny::Object* o, const XEvent& e, void* d) {
        on_minimize_click(o, e, d);
    });

    cls_btn.on_click.connect([this](tiny::Object* o, const XEvent& e, void* d) {
        on_close_click(o, e, d);
    });

    if (is_resizable()) {
        shadow.on_move_resize_begin.connect(
                [this](tiny::Object* o, const XEvent& e, void* d) {
                    on_move_resize_begin(o, e, d);
                });
        shadow.on_move_resize_motion.connect(
                [this](tiny::Object* o, const XEvent& e, void* d) {
                    on_move_resize_motion(o, e, d);
                });
        shadow.on_move_resize_end.connect(
                [this](tiny::Object* o, const XEvent& e, void* d) {
                    on_move_resize_end(o, e, d);
                });
    }
    // the button can come later with the hints
    max_btn.on_click.connect([this](tiny::Object* o, const XEvent& e, void* d) {
        on_maximize_click(o, e, d);
    });
}

void Client::map_all()
{
    if (is_resizable()) {
        shadow.map_all();
    }
    tiny::Container::map_all();
    apply_decorations(); // map_all showed the hidden ones too

    if (is_resizable()) {
        XRaiseWindow(display, shadow.get_window()); // shadow is lower
    }
    XRaiseWindow(display, window);
}

void Client::layout_buttons()
{
    const int padding = header.get_padding();
    int x = header.get_width() - 1; // like Box::push_back
    const struct {
        tiny::Widget* button;
        bool shown;
    } row[] = {{&cls_btn, has_close_button()},
               {&max_btn, has_maximize_button()},
               {&min_btn, has_minimize_button()}};
    for (const auto& item : row) {
        if (!item.shown) {
            item.button->unmap();
            continue;
        }
        x -= padding + item.button->get_width();
        XMoveWindow(display, item.button->get_window(), x,
                    padding - item.button->get_border());
        if (header.get_maped()) {
            item.button->map();
        }
    }
}

int Client::get_border() const
{
    return (decorations & MotifWMHints::DECOR_BORDER)
                   ? tiny::Container::get_border()
                   : 0;
}

int Client::header_height() const
{
    return header_height_for(decorations);
}

void Client::apply_decorations()
{
    if (has_title(decorations)) {
        header.map();
    } else {
        header.unmap();
    }
    if (is_resizable()) {
        if ((decorations & MotifWMHints::DECOR_RESIZE) && !is_fullscreen() &&
            !is_shaded()) {
            shadow.map();
        } else {
            shadow.unmap();
        }
    }
    layout_buttons();
}

void Client::shade(bool on)
{
    if (on == is_shaded()) {
        return;
    }
    if (on) {
        if (!header_height() || get_maximized() || is_fullscreen()) {
            return; // nothing is left to show, or the window fills the area
        }
        XWindowAttributes frame;
        XGetWindowAttributes(display, window, &frame);
        unshaded_height = frame.height;
        XResizeWindow(display, window, tiny::x_dim(frame.width),
                      tiny::x_dim(header_height()));
        if (is_resizable()) {
            shadow.resize(frame.width, header_height());
            shadow.unmap(); // there is nothing to drag
        }
    } else {
        XWindowAttributes frame;
        XGetWindowAttributes(display, window, &frame);
        XResizeWindow(display, window, tiny::x_dim(frame.width),
                      tiny::x_dim(unshaded_height));
        if (is_resizable()) {
            shadow.resize(frame.width, unshaded_height);
        }
    }
    set_shaded(on);
    if (!on && !get_minimized()) {
        apply_decorations();
    }
    send_configure_notify();
}

void Client::on_motif_hints_changed(unsigned long new_decorations)
{
    shade(false); // the sizes below are of the whole window
    if (!new_decorations) {
        on_reframe(nullptr,
                   XEvent()); // a window without a frame is another kind
        return;
    }
    const int old_header = header_height();
    decorations = new_decorations;
    const int delta = header_height() - old_header;
    if (!is_fullscreen()) {
        if (get_maximized()) {
            place_maximized();
        } else {
            XWindowAttributes frame;
            XGetWindowAttributes(display, window, &frame);
            // the content stays where it is, the title bar comes or goes
            XSetWindowBorderWidth(display, window, tiny::x_uint(get_border()));
            XMoveResizeWindow(display, window, frame.x, frame.y - delta,
                              tiny::x_dim(frame.width),
                              tiny::x_dim(frame.height + delta));
            XMoveWindow(display, child, 0, header_height());
            if (is_resizable()) {
                shadow.move_resize(frame.x, frame.y - delta, frame.width,
                                   frame.height + delta);
            }
            send_configure_notify();
        }
    }
    if (!get_minimized()) {
        apply_decorations();
    }
    publish_frame_extents();
}

void Client::set_focus()
{
    raise();
    Window::set_focus();
}

void Client::raise()
{
    if (is_resizable()) {
        XRaiseWindow(display, shadow.get_window()); // shadow is lower
    }
    XRaiseWindow(display, window);
}

void Client::lower()
{
    XLowerWindow(display, window);
    if (is_resizable()) {
        XLowerWindow(display, shadow.get_window()); // shadow is lower
    }
}

void Client::stack_under(::Window sibling)
{
    XWindowChanges changes = {};
    changes.sibling = sibling;
    changes.stack_mode = Below;
    XConfigureWindow(display, window, CWSibling | CWStackMode, &changes);
    if (is_resizable()) {
        changes.sibling = window;
        XConfigureWindow(display, shadow.get_window(), CWSibling | CWStackMode,
                         &changes); // under the frame
    }
}

static FrameExtents extents_with(int header, int border)
{
    return {border, border, header + border, border};
}

FrameExtents Client::frame_extents() const
{
    if (is_fullscreen()) {
        return {}; // the client covers the whole frame
    }
    if (get_maximized()) {
        return {0, 0, header_height(), 0}; // no border
    }
    return extents_with(header_height(), get_border());
}

FrameExtents Client::frame_extents_for(unsigned long decorations)
{
    return extents_with(header_height_for(decorations),
                        border_for(decorations));
}

FrameExtents Client::default_frame_extents()
{
    return frame_extents_for(~0UL);
}

void Client::move_to(int x, int y)
{
    if (is_resizable()) {
        shadow.move(x, y);
    }
    XMoveWindow(display, window, x, y);
    send_configure_notify();
}

void Client::minimize()
{
    // TODO: set state
    set_minimized(true);
    unmap();
    XSetInputFocus(display, parent, RevertToPointerRoot, CurrentTime);
}

void Client::maximize()
{
    if (get_maximized()) {
        return;
    }
    shade(false);

    XWindowAttributes attrs;
    XGetWindowAttributes(dsp, window, &attrs);
    state.x = attrs.x;
    state.y = attrs.y;
    state.width = width;
    state.height = height;

    place_maximized();
    if (has_maximize_button()) {
        max_btn.set_restore(true);
    }
    set_maximized(true);
}

void Client::place_maximized()
{
    XSetWindowBorderWidth(display, window, 0);

    // in the work area: below the panel and the docks, over our own dock
    const tiny::Rect& area = work_area;
    XMoveResizeWindow(display, window, area.x, area.y, tiny::x_uint(area.width),
                      tiny::x_uint(area.height));
    XResizeWindow(display, child, tiny::x_uint(area.width),
                  tiny::x_uint(area.height - header_height()));

    if (is_resizable()) {
        shadow.move_resize(area.x, area.y, area.width, area.height);
    }
    header.resize(area.width, shell_theme.win_header);
    send_configure_notify();
}

void Client::keep_in_work_area()
{
    XWindowAttributes frame;
    if (!XGetWindowAttributes(display, window, &frame)) {
        return;
    }
    const tiny::Rect area = inner_area(work_area, get_border());
    tiny::Rect place;
    if (is_shaded()) {
        // only the title bar is left, it must not stay under a dock; the
        // height comes back with unshade
        place = fit_in_area({frame.x, frame.y, frame.width, frame.height},
                            area);
        place.height = frame.height;
        place.y = std::clamp(frame.y, area.y,
                             area.y + std::max(area.height - frame.height, 0));
    } else {
        place = fit_in_area({frame.x, frame.y, frame.width, frame.height},
                            area);
    }
    if (place.x == frame.x && place.y == frame.y &&
        place.width == frame.width && place.height == frame.height) {
        return;
    }
    if (place.width != frame.width || place.height != frame.height) {
        XResizeWindow(display, child, tiny::x_dim(place.width),
                      tiny::x_dim(place.height - header_height()));
        header.resize(place.width, shell_theme.win_header);
    }
    XMoveResizeWindow(display, window, place.x, place.y,
                      tiny::x_dim(place.width), tiny::x_dim(place.height));
    if (is_resizable() && !is_shaded()) {
        shadow.move_resize(place.x, place.y, place.width, place.height);
    }
    send_configure_notify();
}

void Client::fullscreen(bool on)
{
    if (on == is_fullscreen()) {
        return;
    }
    shade(false);

    if (on) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, window, &attrs);
        before_fullscreen = {attrs.x, attrs.y, attrs.width, attrs.height};
        XGetWindowAttributes(display, root, &attrs);

        // the client covers the frame with its header, so nothing is seen
        XSetWindowBorderWidth(display, window, 0);
        XMoveResizeWindow(display, window, 0, 0, tiny::x_uint(attrs.width),
                          tiny::x_uint(attrs.height));
        XMoveResizeWindow(display, child, 0, 0, tiny::x_uint(attrs.width),
                          tiny::x_uint(attrs.height));
        if (is_resizable()) {
            shadow.unmap();
        }
        XRaiseWindow(display, window);
    } else if (get_maximized()) {
        // the work area can have changed meanwhile
        set_fullscreen(false);
        if (is_resizable() && (decorations & MotifWMHints::DECOR_RESIZE)) {
            shadow.map();
            XRaiseWindow(display, shadow.get_window()); // shadow is lower
            XRaiseWindow(display, window);
        }
        place_maximized();
        return;
    } else {
        const tiny::Rect& r = before_fullscreen;
        XSetWindowBorderWidth(display, window, tiny::x_uint(get_border()));
        XMoveResizeWindow(display, window, r.x, r.y, tiny::x_uint(r.width),
                          tiny::x_uint(r.height));
        XMoveResizeWindow(display, child, 0, header_height(),
                          tiny::x_uint(r.width),
                          tiny::x_uint(r.height - header_height()));
        if (is_resizable()) {
            shadow.move_resize(r.x, r.y, r.width, r.height);
            if (decorations & MotifWMHints::DECOR_RESIZE) {
                shadow.map();
            }
            XRaiseWindow(display, shadow.get_window()); // shadow is lower
            XRaiseWindow(display, window);
        }
    }
    set_fullscreen(on);
    send_configure_notify();
}

void Client::configure_request(const XConfigureRequestEvent& e)
{
    if (get_maximized() || is_fullscreen() || is_shaded()) {
        send_configure_notify(); // the frame keeps its place
        return;
    }

    const int title = header_height();
    XWindowAttributes frame;
    XGetWindowAttributes(display, window, &frame);
    // The client asks for the place of its own area (the position it gets in
    // ConfigureNotify), the frame is around it. The header stays below the
    // panel.
    int new_width = (e.value_mask & CWWidth) ? e.width : frame.width;
    int client_height =
            (e.value_mask & CWHeight) ? e.height : frame.height - title;
    fit_requested_size(e.value_mask, new_width, client_height);
    const int new_height = client_height + title;
    // A size change without a position keeps the reference point of the
    // gravity of the window where it was (ICCCM 4.1.2.3)
    const auto shift =
            gravity_shift(window_gravity(*hints), new_width - frame.width,
                          new_height - frame.height);
    // a window that grows to the west or north stays inside the work area
    int x = frame.x + shift.first;
    if (shift.first < 0) {
        x = std::max(x, work_area.x + frame.border_width);
    }
    int y = frame.y + shift.second;
    if (shift.second < 0) {
        y = std::max(y, work_area.y + frame.border_width);
    }
    if (e.value_mask & CWX) {
        x = std::max(e.x - frame.border_width,
                     work_area.x + frame.border_width);
    }
    if (e.value_mask & CWY) {
        y = std::max(e.y - title - frame.border_width,
                     work_area.y + frame.border_width);
    }

    XMoveResizeWindow(display, window, x, y, tiny::x_dim(new_width),
                      tiny::x_dim(new_height));
    XResizeWindow(display, child, tiny::x_dim(new_width),
                  tiny::x_dim(new_height - title));
    if (is_resizable()) {
        shadow.move_resize(x, y, new_width, new_height);
    }
    header.resize(new_width, shell_theme.win_header);
    if ((e.value_mask & CWStackMode) && e.detail == Above) {
        if (is_resizable()) {
            XRaiseWindow(display, shadow.get_window());
        }
        XRaiseWindow(display, window); // the frame, not the client
    }
    send_configure_notify();
}

void Client::restore(int x, int y)
{
    if (get_maximized()) {
        // the window returns inside of the work area, which may have changed
        const int border = get_border();
        const tiny::Rect place =
                fit_in_area({x ? (x - state.width / 2) : state.x,
                             y ? (y - state.height / 2) : state.y, state.width,
                             state.height},
                            {work_area.x + border, work_area.y + border,
                             std::max(work_area.width - 2 * border, 1),
                             std::max(work_area.height - 2 * border, 1)});
        x = place.x;
        y = place.y;
        state.width = place.width;
        state.height = place.height;

        if (is_resizable()) {
            shadow.move_resize(x, y, state.width, state.height);
        }
        header.resize(state.width, shell_theme.win_header);
        XResizeWindow(display, child, tiny::x_uint(state.width),
                      tiny::x_uint(state.height - header_height()));
        XMoveResizeWindow(display, window, x, y, tiny::x_uint(state.width),
                          tiny::x_uint(state.height));
        XSetWindowBorderWidth(display, window, tiny::x_uint(get_border()));

        if (has_maximize_button()) {
            max_btn.set_restore(false);
        }
        set_maximized(false);
        send_configure_notify();
    } else {
        map();
        set_minimized(false);
    }
}

void Client::on_minimize_click(tiny::Object*, const XEvent&, void*)
{
    minimize();
}

void Client::on_close_click(tiny::Object*, const XEvent&, void*)
{
    close();
}

void Client::on_maximize_click(tiny::Object*, const XEvent&, void*)
{
    if (get_maximized()) {
        restore();
    } else {
        maximize();
    }
}

void Client::on_move_resize_begin(tiny::Object*, const XEvent& e, void*)
{
    set_maximized(false);
    start_event = e;
    XGetWindowAttributes(display, window, &start_attrs);
    long int supplied;
    if (!XGetWMNormalHints(display, child, hints, &supplied)) {
        hints->flags = 0;
    }

    if (has_resize_increment(*hints)) {
        size_label = std::make_unique<SizeLabel>(1, 1);
        size_label->realize(window, 0, 0);
        size_label->set_events();
        size_label->map();
        XRaiseWindow(display, size_label->get_window());
        show_size_units(start_attrs.width,
                        start_attrs.height - header_height());
    }
}

// cppcheck-suppress constParameterPointer ; the signature of a handler
void Client::on_move_resize_motion(tiny::Object*, const XEvent& e, void* data)
{
    auto mask = static_cast<uint16_t>(reinterpret_cast<size_t>(data));
    const int title = header_height();
    const tiny::Rect frame = resize_by_edges(
            *hints,
            {start_attrs.x, start_attrs.y, start_attrs.width,
             start_attrs.height},
            mask, e.xbutton.x_root - start_event.xbutton.x_root,
            e.xbutton.y_root - start_event.xbutton.y_root, title,
            shell_theme.win_min_width, shell_theme.win_min_height);
    const int x = frame.x;
    const int y = frame.y;
    const int new_width = frame.width;
    const int new_height = frame.height;

    if (mask & tiny::Position::Top || mask & tiny::Position::Left) {
        XMoveResizeWindow(display, window, x, y, tiny::x_dim(new_width),
                          tiny::x_dim(new_height));
        shadow.move_resize(x, y, new_width, new_height);
    } else {
        XResizeWindow(display, window, tiny::x_dim(new_width),
                      tiny::x_dim(new_height));
        shadow.resize(new_width, new_height);
    }

    XResizeWindow(display, child, tiny::x_uint(new_width),
                  tiny::x_dim(new_height - title));
    header.resize(new_width, shell_theme.win_header);
    if (size_label) {
        show_size_units(new_width, new_height - title);
    }
}

void Client::on_move_resize_end(tiny::Object*, const XEvent&, void*)
{
    size_label.reset();
    send_configure_notify();
}

void Client::show_size_units(int client_width, int client_height)
{
    size_label->set_area(client_width, client_height, header_height());
    size_label->set_text(
            std::to_string(client_size_units(*hints, true, client_width)) +
            " × " +
            std::to_string(client_size_units(*hints, false, client_height)));
}

void Client::on_window_drag_begin(tiny::Object* o, const XEvent& e, void* data)
{
    const bool twice =
            is_double_click(has_last_click, last_click_time, e.xbutton.time);
    has_last_click = !twice;
    last_click_time = e.xbutton.time;
    Window::on_window_drag_begin(o, e, data);
    XGetWindowAttributes(display, window, &start_attrs);
    if (twice) {
        shade(!is_shaded());
    }
}

void Client::on_window_drag_motion(tiny::Object*, const XEvent& e, void*)
{
    const int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    const int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

    if (is_resizable()) {
        shadow.move(start_attrs.x + xdiff, start_attrs.y + ydiff);
    }
    XMoveWindow(display, window, start_attrs.x + xdiff, start_attrs.y + ydiff);
}

void Client::on_window_drag_end(tiny::Object*, const XEvent&, void*)
{
    send_configure_notify();
}

std::string Client::title_text() const
{
    return responding ? wm_name : wm_name + " (not responding)";
}

void Client::on_responding_changed()
{
    header.set_title(title_text());
}

void Client::begin_drag(unsigned edges, int x_root, int y_root)
{
    drag_edges = edges;
    const XEvent e = make_pointer_event(x_root, y_root);
    if (edges) {
        on_move_resize_begin(nullptr, e, nullptr);
        return;
    }
    Window::on_window_drag_begin(nullptr, e,
                                 nullptr); // a double click is not one
    XGetWindowAttributes(display, window, &start_attrs);
}

void Client::drag_to(int x_root, int y_root)
{
    const XEvent e = make_pointer_event(x_root, y_root);
    if (drag_edges) {
        // NOLINTNEXTLINE(performance-no-int-to-ptr): the mask travels as data
        void* mask = reinterpret_cast<void*>(
                static_cast<size_t>(static_cast<uint16_t>(drag_edges)));
        on_move_resize_motion(nullptr, e, mask);
    } else {
        on_window_drag_motion(nullptr, e, nullptr);
    }
}

void Client::end_drag()
{
    if (drag_edges) {
        on_move_resize_end(nullptr, XEvent(), nullptr);
    } else {
        on_window_drag_end(nullptr, XEvent(), nullptr);
    }
    drag_edges = 0;
}

void Client::on_focus_in(const XEvent& e, void* data)
{
    if (!is_focus_change(e)) {
        return;
    }
    header.set_disable(false);
    Window::on_focus_in(e, data);
}

void Client::on_focus_out(const XEvent& e, void* data)
{
    if (!is_focus_change(e)) {
        return;
    }
    header.set_disable(true);
    Window::on_focus_out(e, data);
}

void Client::on_property_notify(const XEvent& e, void* data)
{
    Window::on_property_notify(e, data);

    if (e.xproperty.atom == display.WM_NAME ||
        e.xproperty.atom == display._NET_WM_NAME) {
        header.set_title(title_text());
    }
}

} // namespace tiny::wm
