#include <X11/cursorfont.h>

#include "x_util.h"
#include "wm_edge.h"

namespace wm {

Edge::Edge(int w, int h, uint16_t mask_arg):
    tiny::Widget(tiny::Widget::Type::Input, w, h), mask(mask_arg)
{
    name = "wm_edge";
}

Edge::~Edge()
{
    if (event_done) {
        XUngrabButton(display, Button1, AnyModifier, window);
    }
}

void Edge::realize(Window parent_arg, int x, int y)
{
    Widget::realize(parent_arg, x, y);
    XDefineCursor(
            display, window,
            XCreateFontCursor(display, tiny::Position::get_cursor_shape(mask)));

    // TODO: tiny::Widget::Type::Debug :-) will set background and check childs
    // uint64_t background = std::rand();
    // XSetWindowBackground(display, window, background);
}

void Edge::set_events(long mask_arg)
{
    Widget::set_events(mask_arg);

    XGrabButton(display, Button1, AnyModifier, window, true,
                ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
                GrabModeAsync, None, XCreateFontCursor(display, XC_hand1));

    connect(ButtonPress,
            static_cast<tiny::event_signal_t>(&Edge::on_button_press));
    connect(ButtonRelease,
            static_cast<tiny::event_signal_t>(&Edge::on_button_release));
    connect(MotionNotify,
            static_cast<tiny::event_signal_t>(&Edge::on_motion_notify));
}

void Edge::on_button_press(const XEvent& e, void*)
{
    XGrabPointer(display, window, false,
                 ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    on_drag_begin(this, e);
}

void Edge::on_button_release(const XEvent& e, void*)
{
    XUngrabPointer(display, CurrentTime);

    on_drag_end(this, e);
}

void Edge::on_motion_notify(const XEvent& e, void*)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent( // skip penging motion events
            display, event.xmotion.window, MotionNotify, &event)) {}

    on_drag_motion(this, event);
}

BackWindow::BackWindow(int w, int h):
    tiny::Container(tiny::Widget::Type::Input,
                    w + 2 * tiny::theme.wm_win_border,
                    h + 2 * tiny::theme.wm_win_border),
    corner_right_top(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
                     tiny::Position::Right | tiny::Position::Top),
    corner_right_bottom(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
                        tiny::Position::Right | tiny::Position::Bottom),
    corner_left_top(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
                    tiny::Position::Left | tiny::Position::Top),
    corner_left_bottom(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
                       tiny::Position::Left | tiny::Position::Bottom),
    edge_top(w - tiny::theme.wm_win_corner, tiny::theme.wm_win_border,
             tiny::Position::Top),
    edge_right(tiny::theme.wm_win_border, h - tiny::theme.wm_win_corner,
               tiny::Position::Right),
    edge_bottom(w - tiny::theme.wm_win_corner, tiny::theme.wm_win_border,
                tiny::Position::Bottom),
    edge_left(tiny::theme.wm_win_border, h - tiny::theme.wm_win_corner,
              tiny::Position::Left)
{
    name = "wm_backwindow";
}

BackWindow::~BackWindow() {}

void BackWindow::set_events(long mask)
{
    tiny::Container::set_events(mask);

    corner_right_top.on_drag_begin.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_begin));
    corner_right_top.on_drag_motion.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_motion));

    corner_right_bottom.on_drag_begin.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_begin));
    corner_right_bottom.on_drag_motion.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_motion));

    corner_left_top.on_drag_begin.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_begin));
    corner_left_top.on_drag_motion.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_motion));

    corner_left_bottom.on_drag_begin.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_begin));
    corner_left_bottom.on_drag_motion.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_motion));

    edge_top.on_drag_begin.connect(this,
                                   static_cast<tiny::object_signal_t>(
                                           &BackWindow::on_edge_drag_begin));
    edge_top.on_drag_motion.connect(this,
                                    static_cast<tiny::object_signal_t>(
                                            &BackWindow::on_edge_drag_motion));

    edge_right.on_drag_begin.connect(this,
                                     static_cast<tiny::object_signal_t>(
                                             &BackWindow::on_edge_drag_begin));
    edge_right.on_drag_motion.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_motion));

    edge_bottom.on_drag_begin.connect(this,
                                      static_cast<tiny::object_signal_t>(
                                              &BackWindow::on_edge_drag_begin));
    edge_bottom.on_drag_motion.connect(
            this, static_cast<tiny::object_signal_t>(
                          &BackWindow::on_edge_drag_motion));

    edge_left.on_drag_begin.connect(this,
                                    static_cast<tiny::object_signal_t>(
                                            &BackWindow::on_edge_drag_begin));
    edge_left.on_drag_motion.connect(this,
                                     static_cast<tiny::object_signal_t>(
                                             &BackWindow::on_edge_drag_motion));
}

void BackWindow::realize(Window parent_arg, int x, int y)
{
    Container::realize(parent_arg, x - tiny::theme.wm_win_border,
                       y - tiny::theme.wm_win_border);

    add(&corner_right_top, width - tiny::theme.wm_win_corner, 0,
        NorthEastGravity);
    add(&corner_right_bottom, width - tiny::theme.wm_win_corner,
        height - tiny::theme.wm_win_corner, SouthEastGravity);
    add(&corner_left_top, 0, 0, NorthWestGravity);
    add(&corner_left_bottom, 0, height - tiny::theme.wm_win_corner,
        SouthWestGravity);

    add(&edge_top, tiny::theme.wm_win_corner, 0, NorthGravity);
    add(&edge_right, width - tiny::theme.wm_win_border,
        tiny::theme.wm_win_corner, EastGravity);
    add(&edge_bottom, tiny::theme.wm_win_corner,
        height - tiny::theme.wm_win_border, SouthGravity);
    add(&edge_left, 0, tiny::theme.wm_win_corner, WestGravity);
}

void BackWindow::resize(int w, int h)
{
    tiny::Container::resize(w + 2 * tiny::theme.wm_win_border,
                            h + 2 * tiny::theme.wm_win_border);
}

void BackWindow::move(int x, int y)
{
    if (is_realized) {
        XMoveWindow(display, window, x - tiny::theme.wm_win_border,
                    y - tiny::theme.wm_win_border);
    }
}

void BackWindow::move_resize(int x, int y, int w, int h)
{
    if (is_realized) {
        XMoveResizeWindow(display, window, x - tiny::theme.wm_win_border,
                          y - tiny::theme.wm_win_border,
                          tiny::x_uint(w + 2 * tiny::theme.wm_win_border),
                          tiny::x_uint(h + 2 * tiny::theme.wm_win_border));
    }
}

void BackWindow::on_edge_drag_begin(Object*, const XEvent& e, void*)
{
    on_move_resize_begin(this, e);
}

void BackWindow::on_edge_drag_motion(Object* o, const XEvent& e, void*)
{
    on_move_resize_motion(
            this, e,
            reinterpret_cast<void*>(static_cast<Edge*>(o)->get_mask()));
}

void BackWindow::on_configure_notify(const XEvent& e, void* data)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent( // skip penging motion events
            display, event.xconfigure.window, ConfigureNotify, &event)) {}

    int old_width = width;
    int old_height = height;
    tiny::Container::on_configure_notify(event, data);
    if (width != old_width || height != old_height) {
        XMoveResizeWindow(display, edge_top.get_window(),
                          tiny::theme.wm_win_corner, 0,
                          tiny::x_uint(width - 2 * tiny::theme.wm_win_corner),
                          tiny::x_uint(tiny::theme.wm_win_border));
        XMoveResizeWindow(display, edge_right.get_window(),
                          width - tiny::theme.wm_win_border,
                          tiny::theme.wm_win_corner,
                          tiny::x_uint(tiny::theme.wm_win_border),
                          tiny::x_uint(height - 2 * tiny::theme.wm_win_corner));
        XMoveResizeWindow(display, edge_bottom.get_window(),
                          tiny::theme.wm_win_corner,
                          height - tiny::theme.wm_win_border,
                          tiny::x_uint(width - 2 * tiny::theme.wm_win_corner),
                          tiny::x_uint(tiny::theme.wm_win_border));
        XMoveResizeWindow(display, edge_left.get_window(), 0,
                          tiny::theme.wm_win_corner,
                          tiny::x_uint(tiny::theme.wm_win_border),
                          tiny::x_uint(height - 2 * tiny::theme.wm_win_corner));
    }
}

} // namespace wm
