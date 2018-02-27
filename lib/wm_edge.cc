#include <X11/cursorfont.h>

#include "wm_edge.h"

namespace wm {

Edge::Edge(uint32_t width, uint32_t height, uint16_t mask):
    tiny::Widget(tiny::Widget::Type::Input, width, height),
    mask(mask)
{}

Edge::~Edge()
{
    XUngrabButton(display, Button1, AnyModifier, window);
}

void Edge::realize(Display * display, Window parent, int x, int y)
{
    Widget::realize(display, parent, x, y);
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
            width+2*WM_WIN_BORDER, height+2*WM_WIN_BORDER),
    corner_right_top(WM_WIN_CORNER, WM_WIN_CORNER,
            tiny::Position::Right|tiny::Position::Top),
    corner_right_bottom(WM_WIN_CORNER, WM_WIN_CORNER,
            tiny::Position::Right|tiny::Position::Bottom),
    corner_left_top(WM_WIN_CORNER, WM_WIN_CORNER,
            tiny::Position::Left|tiny::Position::Top),
    corner_left_bottom(WM_WIN_CORNER, WM_WIN_CORNER,
            tiny::Position::Left|tiny::Position::Bottom),
    edge_top(width-WM_WIN_CORNER, WM_WIN_BORDER, tiny::Position::Top),
    edge_right(WM_WIN_BORDER, height-WM_WIN_CORNER, tiny::Position::Right),
    edge_bottom(width-WM_WIN_CORNER, WM_WIN_BORDER, tiny::Position::Bottom),
    edge_left(WM_WIN_BORDER, height-WM_WIN_CORNER, tiny::Position::Left)
{}

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

void BackWindow::realize(Display * display, Window parent, int x, int y)
{
    Container::realize(display, parent, x-WM_WIN_BORDER, y-WM_WIN_BORDER);

    add(&corner_right_top, width-WM_WIN_CORNER, 0,
            NorthEastGravity);
    add(&corner_right_bottom, width-WM_WIN_CORNER, height-WM_WIN_CORNER,
            SouthEastGravity);
    add(&corner_left_top, 0, 0,
            NorthWestGravity);
    add(&corner_left_bottom, 0, height-WM_WIN_CORNER,
            SouthWestGravity);

    add(&edge_top, WM_WIN_CORNER, 0, NorthGravity);
    add(&edge_right, width-WM_WIN_BORDER, WM_WIN_CORNER, EastGravity);
    add(&edge_bottom, WM_WIN_CORNER, height-WM_WIN_BORDER, SouthGravity);
    add(&edge_left, 0, WM_WIN_CORNER, WestGravity);
}

void BackWindow::resize(uint32_t width, uint32_t height)
{
    tiny::Container::resize(width+2*WM_WIN_BORDER, height+2*WM_WIN_BORDER);
}

void BackWindow::move(int x, int y){
    XMoveWindow(display, window, x-WM_WIN_BORDER, y-WM_WIN_BORDER);
}

void BackWindow::move_resize(int x, int y, uint32_t width, uint32_t height)
{
    XMoveResizeWindow(display, window, x-WM_WIN_BORDER, y-WM_WIN_BORDER,
            width+2*WM_WIN_BORDER, height+2*WM_WIN_BORDER);
}

void BackWindow::on_edge_drag_begin(Object *o, const XEvent &e, void *data)
{
    on_move_resize_begin(this, e);
}

void BackWindow::on_edge_drag_motion(Object *o, const XEvent &e, void *data)
{
    uint16_t mask = 0;
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
                WM_WIN_CORNER, 0,
                width-2*WM_WIN_CORNER, WM_WIN_BORDER);
        XMoveResizeWindow(display, edge_right.get_window(),
                width-WM_WIN_BORDER, WM_WIN_CORNER,
                WM_WIN_CORNER, height-2*WM_WIN_CORNER);
        XMoveResizeWindow(display, edge_bottom.get_window(),
                WM_WIN_CORNER, height-WM_WIN_BORDER,
                width-2*WM_WIN_CORNER, WM_WIN_BORDER);
        XMoveResizeWindow(display, edge_left.get_window(),
                0, WM_WIN_CORNER,
                WM_WIN_BORDER, height-2*WM_WIN_CORNER);
    }
}

} // namespace wm
