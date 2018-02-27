#include <X11/cursorfont.h>

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


Container::Container(uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Widget(Widget::Type::Normal, width, height, border, border_color, background)
{}

Container::Container(Widget::Type type, uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Widget(type, width, height, border, border_color, background)
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
        throw std::runtime_error("Box must be realized");
    }
    widget->realize(display, window, x, y);
    if (gravity != NorthWestGravity){
        XSetWindowAttributes attrs;
        attrs.win_gravity = gravity;
        XChangeWindowAttributes(display, widget->get_window(), CWWinGravity,
                &attrs);
    }
    children.push_back(widget);
}



Box::Box(Type type, uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Container(width, height, border, border_color, background),
    type(type), start_offset(0)
{
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

} // namespace tiny
