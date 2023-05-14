#include <X11/cursorfont.h>

#include "wm_edge.h"

namespace wm {

Edge::Edge(uint32_t width, uint32_t height, uint16_t mask):
    tiny::Widget(tiny::Widget::Type::Input, width, height),
    mask(mask)
{
    name = "wm_edge";
}

Edge::~Edge()
{
    if (event_done){
        XUngrabButton(display, Button1, AnyModifier, window);
    }
}

void Edge::realize(Window parent, int x, int y)
{
    Widget::realize(parent, x, y);
    XDefineCursor(display, window,
            XCreateFontCursor(display, tiny::Position::get_cursor_shape(mask)));

    // TODO: tiny::Widget::Type::Debug :-) will set background and check childs
    // uint64_t background = std::rand();
    // XSetWindowBackground(display, window, background);
}

void Edge::set_events(long mask)
{
    Widget::set_events(mask);

    XGrabButton(
        display, Button1, AnyModifier, window, true,
        ButtonPressMask | ButtonReleaseMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1));

    connect(ButtonPress,
            static_cast<tiny::event_signal_t>(&Edge::on_button_press));
    connect(ButtonRelease,
            static_cast<tiny::event_signal_t>(&Edge::on_button_release));
    connect(MotionNotify,
            static_cast<tiny::event_signal_t>(&Edge::on_motion_notify));
}


void Edge::on_button_press(const XEvent &e, void *data)
{
    XGrabPointer(
        display, window, false,
        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
        GrabModeAsync, GrabModeAsync,
        None, None, CurrentTime);

    on_drag_begin(this, e);
}

void Edge::on_button_release(const XEvent &e, void *data)
{
    XUngrabPointer(display, CurrentTime);

    on_drag_end(this, e);
}

void Edge::on_motion_notify(const XEvent &e, void *data)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent(  // skip penging motion events
            display, event.xmotion.window, MotionNotify, &event))
    {}

    on_drag_motion(this, event);
}



BackWindow::BackWindow(uint32_t width, uint32_t height):
    tiny::Container(tiny::Widget::Type::Input,
            width+2*tiny::theme.wm_win_border, height+2*tiny::theme.wm_win_border),
    corner_right_top(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
            tiny::Position::Right|tiny::Position::Top),
    corner_right_bottom(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
            tiny::Position::Right|tiny::Position::Bottom),
    corner_left_top(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
            tiny::Position::Left|tiny::Position::Top),
    corner_left_bottom(tiny::theme.wm_win_corner, tiny::theme.wm_win_corner,
            tiny::Position::Left|tiny::Position::Bottom),
    edge_top(width-tiny::theme.wm_win_corner, tiny::theme.wm_win_border,
            tiny::Position::Top),
    edge_right(tiny::theme.wm_win_border, height-tiny::theme.wm_win_corner,
            tiny::Position::Right),
    edge_bottom(width-tiny::theme.wm_win_corner, tiny::theme.wm_win_border,
            tiny::Position::Bottom),
    edge_left(tiny::theme.wm_win_border, height-tiny::theme.wm_win_corner,
            tiny::Position::Left)
{
    name = "wm_backwindow";
}

BackWindow::~BackWindow()
{}

void BackWindow::set_events(long mask)
{
    tiny::Container::set_events(mask);

    corner_right_top.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    corner_right_top.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));

    corner_right_bottom.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    corner_right_bottom.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));

    corner_left_top.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    corner_left_top.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));

    corner_left_bottom.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    corner_left_bottom.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));

    edge_top.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    edge_top.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));

    edge_right.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    edge_right.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));

    edge_bottom.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    edge_bottom.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));

    edge_left.on_drag_begin.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_begin));
    edge_left.on_drag_motion.connect(this,
        static_cast<tiny::object_signal_t>(&BackWindow::on_edge_drag_motion));
}

void BackWindow::realize(Window parent, int x, int y)
{
    Container::realize(parent,
            x-tiny::theme.wm_win_border, y-tiny::theme.wm_win_border);

    add(&corner_right_top, width-tiny::theme.wm_win_corner, 0,
            NorthEastGravity);
    add(&corner_right_bottom,
            width-tiny::theme.wm_win_corner,
            height-tiny::theme.wm_win_corner,
            SouthEastGravity);
    add(&corner_left_top, 0, 0,
            NorthWestGravity);
    add(&corner_left_bottom, 0, height-tiny::theme.wm_win_corner,
            SouthWestGravity);

    add(&edge_top, tiny::theme.wm_win_corner, 0, NorthGravity);
    add(&edge_right, width-tiny::theme.wm_win_border, tiny::theme.wm_win_corner,
            EastGravity);
    add(&edge_bottom, tiny::theme.wm_win_corner, height-tiny::theme.wm_win_border,
            SouthGravity);
    add(&edge_left, 0, tiny::theme.wm_win_corner, WestGravity);
}

void BackWindow::resize(uint32_t width, uint32_t height)
{
    tiny::Container::resize(width+2*tiny::theme.wm_win_border,
                            height+2*tiny::theme.wm_win_border);
}

void BackWindow::move(int x, int y){
    if (is_realized){
        XMoveWindow(display, window,
                x-tiny::theme.wm_win_border, y-tiny::theme.wm_win_border);
    }
}

void BackWindow::move_resize(int x, int y, uint32_t width, uint32_t height)
{
    if (is_realized){
        XMoveResizeWindow(display, window,
                x-tiny::theme.wm_win_border, y-tiny::theme.wm_win_border,
                width+2*tiny::theme.wm_win_border,
                height+2*tiny::theme.wm_win_border);
    }
}

void BackWindow::on_edge_drag_begin(Object *o, const XEvent &e, void *data)
{
    on_move_resize_begin(this, e);
}

void BackWindow::on_edge_drag_motion(Object *o, const XEvent &e, void *data)
{
    on_move_resize_motion(this, e,
            reinterpret_cast<void*>(static_cast<Edge*>(o)->get_mask()));
}

void BackWindow::on_configure_notify(const XEvent &e, void * data)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent(  // skip penging motion events
            display, event.xconfigure.window, ConfigureNotify, &event))
    {}

    uint32_t old_width = width;
    uint32_t old_height = height;
    tiny::Container::on_configure_notify(event, data);
    if (width != old_width || height != old_height){
        XMoveResizeWindow(display, edge_top.get_window(),
                tiny::theme.wm_win_corner, 0,
                width-2*tiny::theme.wm_win_corner, tiny::theme.wm_win_border);
        XMoveResizeWindow(display, edge_right.get_window(),
                width-tiny::theme.wm_win_border, tiny::theme.wm_win_corner,
                tiny::theme.wm_win_corner, height-2*tiny::theme.wm_win_corner);
        XMoveResizeWindow(display, edge_bottom.get_window(),
                tiny::theme.wm_win_corner, height-tiny::theme.wm_win_border,
                width-2*tiny::theme.wm_win_corner, tiny::theme.wm_win_border);
        XMoveResizeWindow(display, edge_left.get_window(),
                0, tiny::theme.wm_win_corner,
                tiny::theme.wm_win_border, height-2*tiny::theme.wm_win_corner);
    }
}

} // namespace wm
