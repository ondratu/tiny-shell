#include <X11/cursorfont.h>

#include <algorithm>
#include <memory>
#include <string>

#include "object.h"
#include "theme.h"
#include "wm/window.h"
#include "wm/client.h"
#include "wm/work_area.h"
#include "x_util.h"

namespace tiny {
extern std::shared_ptr<Display> display;
}

namespace tiny::wm {

Client::Client(::Window child_arg, ::Window root_arg, int w, int h,
               unsigned long functions_arg, unsigned long decorations_arg):
    tiny::Container(w, h + tiny::theme.wm_win_header, "wm_window"),
    Window(child_arg, root_arg, functions_arg),
    decorations(decorations_arg),
    header(w, tiny::theme.wm_win_header),
    shadow(w, h + tiny::theme.wm_win_header)
{}

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
                        attrs.y + tiny::theme.wm_win_header);
        XSync(display, False);
        XSetErrorHandler(old);
    }

    tiny::Container::disconnect_window(PropertyNotify, child);

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
                              const tiny::Rect& area)
{
    const int border = 1; // of the frame, it is on both sides
    const int header = tiny::theme.wm_win_header;
    // the border is drawn outside of the window, so it is not over the panel
    const tiny::Rect inner = {area.x + border, area.y + border,
                              std::max(area.width - 2 * border, 1),
                              std::max(area.height - 2 * border, 1)};
    return fit_in_area({attrs.x, attrs.y, attrs.width, attrs.height + header},
                       inner);
}

Window* Client::create(::Window root, ::Window child,
                       const XWindowAttributes& attrs, unsigned long functions,
                       unsigned long decorations, const tiny::Rect& area)
{
    const tiny::Rect place = first_place(attrs, area);
    const int client_height = place.height - tiny::theme.wm_win_header;
    auto* win = new Client(child, root, place.width, client_height, functions,
                           decorations);
    win->work_area = area;
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
    XReparentWindow(display, child, window, 0, tiny::theme.wm_win_header);
    header.set_title(wm_name);
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
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(
                    &Client::on_window_drag_begin));
    header.get_title_box()->on_drag_motion.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(
                    &Client::on_window_drag_motion));

    header.get_title_box()->on_drag_end.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(
                    &Client::on_window_drag_end));

    min_btn.on_click.connect(reinterpret_cast<tiny::Object*>(this),
                             reinterpret_cast<tiny::object_signal_t>(
                                     &Client::on_minimize_click));

    cls_btn.on_click.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Client::on_close_click));

    if (is_resizable()) {
        shadow.on_move_resize_begin.connect(
                reinterpret_cast<tiny::Object*>(this),
                reinterpret_cast<tiny::object_signal_t>(
                        &Client::on_move_resize_begin));
        shadow.on_move_resize_motion.connect(
                reinterpret_cast<tiny::Object*>(this),
                reinterpret_cast<tiny::object_signal_t>(
                        &Client::on_move_resize_motion));
        shadow.on_move_resize_end.connect(
                reinterpret_cast<tiny::Object*>(this),
                reinterpret_cast<tiny::object_signal_t>(
                        &Client::on_move_resize_end));
        max_btn.on_click.connect(reinterpret_cast<tiny::Object*>(this),
                                 reinterpret_cast<tiny::object_signal_t>(
                                         &Client::on_maximize_click));
    }
}

void Client::map_all()
{
    if (is_resizable()) {
        shadow.map_all();
    }
    tiny::Container::map_all();
    layout_buttons(); // map_all showed the hidden ones too

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

void Client::on_motif_hints_changed(unsigned long new_decorations)
{
    decorations = new_decorations;
    layout_buttons();
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

static FrameExtents extents_with(int border)
{
    return {border, border, tiny::theme.wm_win_header + border, border};
}

FrameExtents Client::frame_extents() const
{
    if (is_fullscreen()) {
        return {}; // the client covers the whole frame
    }
    if (get_maximized()) {
        return {0, 0, tiny::theme.wm_win_header, 0}; // no border
    }
    return extents_with(get_border());
}

FrameExtents Client::default_frame_extents()
{
    return extents_with(tiny::theme.widget.border_width);
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
                  tiny::x_uint(area.height - tiny::theme.wm_win_header));

    if (is_resizable()) {
        shadow.move_resize(area.x, area.y, area.width, area.height);
    }
    header.resize(area.width, tiny::theme.wm_win_header);
    send_configure_notify();
}

void Client::fullscreen(bool on)
{
    if (on == is_fullscreen()) {
        return;
    }

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
        if (is_resizable()) {
            shadow.map();
            XRaiseWindow(display, shadow.get_window()); // shadow is lower
            XRaiseWindow(display, window);
        }
        place_maximized();
        return;
    } else {
        const tiny::Rect& r = before_fullscreen;
        XSetWindowBorderWidth(display, window, 1);
        XMoveResizeWindow(display, window, r.x, r.y, tiny::x_uint(r.width),
                          tiny::x_uint(r.height));
        XMoveResizeWindow(display, child, 0, tiny::theme.wm_win_header,
                          tiny::x_uint(r.width),
                          tiny::x_uint(r.height - tiny::theme.wm_win_header));
        if (is_resizable()) {
            shadow.move_resize(r.x, r.y, r.width, r.height);
            shadow.map();
            XRaiseWindow(display, shadow.get_window()); // shadow is lower
            XRaiseWindow(display, window);
        }
    }
    set_fullscreen(on);
    send_configure_notify();
}

void Client::configure_request(const XConfigureRequestEvent& e)
{
    if (get_maximized() || is_fullscreen()) {
        send_configure_notify(); // the frame keeps its place
        return;
    }

    const int header_height = tiny::theme.wm_win_header;
    XWindowAttributes frame;
    XGetWindowAttributes(display, window, &frame);
    // The client asks for the place of its own area (the position it gets in
    // ConfigureNotify), the frame is around it. The header stays below the
    // panel.
    const int x = (e.value_mask & CWX)
                          ? std::max(e.x - frame.border_width,
                                     work_area.x + frame.border_width)
                          : frame.x;
    const int y = (e.value_mask & CWY)
                          ? std::max(e.y - header_height - frame.border_width,
                                     work_area.y + frame.border_width)
                          : frame.y;
    const int new_width = (e.value_mask & CWWidth) ? e.width : frame.width;
    const int new_height =
            (e.value_mask & CWHeight) ? e.height + header_height : frame.height;

    XMoveResizeWindow(display, window, x, y, tiny::x_dim(new_width),
                      tiny::x_dim(new_height));
    XResizeWindow(display, child, tiny::x_dim(new_width),
                  tiny::x_dim(new_height - header_height));
    if (is_resizable()) {
        shadow.move_resize(x, y, new_width, new_height);
    }
    header.resize(new_width, header_height);
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
        // TODO: stay in desktop
        x = x ? (x - state.width / 2) : state.x;
        y = y ? (y - state.height / 2) : state.y;

        if (is_resizable()) {
            shadow.move_resize(x, y, state.width, state.height);
        }
        header.resize(state.width, tiny::theme.wm_win_header);
        XResizeWindow(display, child, tiny::x_uint(state.width),
                      tiny::x_uint(state.height - tiny::theme.wm_win_header));
        XMoveResizeWindow(display, window, x, y, tiny::x_uint(state.width),
                          tiny::x_uint(state.height));
        XSetWindowBorderWidth(display, window, 1);

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
                        start_attrs.height - tiny::theme.wm_win_header);
    }
}

void Client::on_move_resize_motion(tiny::Object*, const XEvent& e, void* data)
{
    auto mask = static_cast<uint16_t>(reinterpret_cast<size_t>(data));
    const int header_height = tiny::theme.wm_win_header;
    int new_width = start_attrs.width;
    int new_height = start_attrs.height;

    if (mask & (tiny::Position::Left | tiny::Position::Right)) {
        const int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
        const int width_arg =
                new_width + (mask & tiny::Position::Right ? xdiff : -xdiff);
        new_width = fit_client_size(*hints, true, width_arg,
                                    tiny::theme.wm_win_min_width);
    }
    if (mask & (tiny::Position::Top | tiny::Position::Bottom)) {
        const int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;
        const int height_arg = new_height - header_height +
                               (mask & tiny::Position::Bottom ? ydiff : -ydiff);
        new_height =
                fit_client_size(*hints, false, height_arg,
                                tiny::theme.wm_win_min_height - header_height) +
                header_height;
    }

    // The edge that is not dragged stays, even when the size is limited
    const int x = (mask & tiny::Position::Left)
                          ? start_attrs.x + start_attrs.width - new_width
                          : start_attrs.x;
    const int y = (mask & tiny::Position::Top)
                          ? start_attrs.y + start_attrs.height - new_height
                          : start_attrs.y;

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
                  tiny::x_dim(new_height - header_height));
    header.resize(new_width, header_height);
    if (size_label) {
        show_size_units(new_width, new_height - header_height);
    }
}

void Client::on_move_resize_end(tiny::Object*, const XEvent&, void*)
{
    size_label.reset();
    send_configure_notify();
}

void Client::show_size_units(int client_width, int client_height)
{
    size_label->set_area(client_width, client_height,
                         tiny::theme.wm_win_header);
    size_label->set_text(
            std::to_string(client_size_units(*hints, true, client_width)) +
            " × " +
            std::to_string(client_size_units(*hints, false, client_height)));
}

void Client::on_window_drag_begin(tiny::Object* o, const XEvent& e, void* data)
{
    Window::on_window_drag_begin(o, e, data);
    XGetWindowAttributes(display, window, &start_attrs);
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
        header.set_title(wm_name);
    }
}

} // namespace tiny::wm
