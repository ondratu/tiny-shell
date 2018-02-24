#include "containers.h"

namespace tiny {

Container::Container(uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Widget(width, height, border, border_color, background),
    ContainerInterface()
{}

Container::~Container()
{}

void Container::map_all(){
    Widget::map_all();

    for(auto w: children){
        w->map_all();
    }
}

void Container::set_events(){
}

void Container::add(Widget * widget, int x, int y, int gravity)
{
    if (!is_realized) {
        throw std::runtime_error("Box must be realized");
    }
    ContainerInterface::add(widget);
    widget->realize(display, window, x, y);
    if (gravity != NorthWestGravity){
        XSetWindowAttributes attrs;
        attrs.win_gravity = gravity;
        XChangeWindowAttributes(display, window, CWWinGravity , &attrs);
    }
}



Box::Box(Type type, uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Container(width, height, border, border_color, background),
    type(type), start_offset(0)
{
    if (type == Type::Horizontal) {
        end_offset = width;
    } else { // Type::Vertical
        end_offset = height;
    }
}

Box::~Box()
{}

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

} // namespace tiny
