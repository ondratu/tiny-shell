#include <X11/cursorfont.h>

#include <sys/time.h>

#include "containers.h"

namespace tiny {

uint32_t Position::get_cursor_shape(uint16_t mask)
{
    switch(mask) {
        case Top:
            return XC_top_side;
        case Right:
            return XC_right_side;
        case Bottom:
            return XC_bottom_side;
        case Left:
            return XC_left_side;

        case Top|Right:
            return XC_top_right_corner;
        case Bottom|Right:
            return XC_bottom_right_corner;
        case Top|Left:
            return XC_top_left_corner;
        case Bottom|Left:
            return XC_bottom_left_corner;

        // TODO: Left|Right
        // TODO: Top|Bottom

        default:
            return XC_arrow;
    }
}

Container::Container(Widget::Type type, uint32_t width, uint32_t height,
        const char* name):
    Widget(type, width, height)
{
    this->name = name;
}


Container::Container(uint32_t width, uint32_t height, const char* name):
    Container(Widget::Type::Normal, width, height, name)
{}


Container::~Container()
{}

void Container::map_all(){
    Widget::map_all();

    for(auto w: children){
        w->map_all();
    }
}

void Container::add(Widget * widget, int x, int y, int gravity)
{
    if (!is_realized) {
        return error("tiny::Container must be realized");
    }
    widget->realize(window, x, y);
    if (gravity != NorthWestGravity){
        XSetWindowAttributes attrs;
        attrs.win_gravity = gravity;
        XChangeWindowAttributes(display, widget->get_window(), CWWinGravity,
                &attrs);
    }
    children.push_back(widget);
}



Box::Box(Type type, uint32_t width, uint32_t height):
    Container(width, height),
    type(type), start_offset(0)
{
    name = "box";
    if (type == Type::Horizontal) {
        end_offset = width-1;
    } else { // Type::Vertical
        end_offset = height-1;
    }
}

Box::~Box()
{}

void Box::resize(uint32_t width, uint32_t height){
    if (type == Type::Horizontal){
        end_offset += width - this->width;
    } else { // Type::Vertical
        end_offset += height - this->height;
    }
    Container::resize(width, height);
}

void Box::push_start(Widget * widget, int x_spacing, int y_spacing, int gravity)
{
    if (type == Type::Horizontal){
        add(widget, start_offset+x_spacing, y_spacing,
                (gravity == AutoGravity ? NorthWestGravity : gravity));
        start_offset += x_spacing + widget->get_width();
    } else {    // Type::Vertical
        add(widget, x_spacing, start_offset+y_spacing,
                (gravity == AutoGravity ? NorthWestGravity : gravity));
        start_offset += y_spacing + widget->get_height();
    }
}

void Box::push_back(Widget * widget, int x_spacing, int y_spacing, int gravity)
{
    if (type == Type::Horizontal){
        end_offset -= x_spacing + widget->get_width();
        add(widget, end_offset, y_spacing,
                (gravity == AutoGravity ? SouthEastGravity : gravity));
    } else {    // Type::Vertical
        end_offset -= y_spacing + widget->get_height();
        add(widget, x_spacing, end_offset,
                (gravity == AutoGravity ? SouthEastGravity : gravity));
    }
}


Popover::Popover(Box::Type type, uint32_t width, uint32_t height):
    Box(type, width, height),
    time_from_unmap(0)
{
    name = "popover";
}


Popover::Popover(uint32_t width, uint32_t height):
    Popover(Box::Type::Vertical, width, height)
{}


Popover::~Popover()
{
    if (event_done) {
        disconnect(FocusOut);
    }
}

void Popover::realize(::Window root, int x, int y)
{
    Box::realize(root, x, y);
    XSetWindowAttributes attrs;
    attrs.override_redirect = true;

    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attrs);
}

void Popover::set_events(long mask){
    connect(FocusOut,
           static_cast<event_signal_t>(&Popover::on_focus_out));

    Box::set_events(mask|FocusChangeMask);
}

void Popover::popup(int x, int y)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    if (ms - time_from_unmap < 200){    // 200 ms timeout
        return;
    }

    // TODO: shift x to could show on root
    // TODO: shift y to could show on root

    x -= width/2;   // center the popover under x

    XMoveWindow(display, window, x, y);
    map_all();
    XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}

void Popover::popup(const XButtonEvent &be){
    popup(be.x_root, be.y_root);
}

void Popover::on_focus_out(const XEvent &e, void *)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    time_from_unmap = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    unmap();
}

} // namespace tiny
