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

Container::Container(Widget::Type type_arg, uint32_t w, uint32_t h,
        const char* name_arg):
    Widget(type_arg, w, h)
{
    this->name = name_arg;
}


Container::Container(uint32_t w, uint32_t h, const char* name_arg):
    Container(Widget::Type::Normal, w, h, name_arg)
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

void Container::remove(Widget* widget)
{
    children.remove(widget);
}


Box::Box(Type type_arg, uint32_t w, uint32_t h):
    Container(w, h),
    type(type_arg), start_offset(0)
{
    name = "box";
    if (type_arg == Type::Horizontal) {
        end_offset = w-1;
    } else { // Type::Vertical
        end_offset = h-1;
    }
}

Box::~Box()
{}

void Box::resize(uint32_t w, uint32_t h){
    if (type == Type::Horizontal){
        end_offset += w - this->width;
    } else { // Type::Vertical
        end_offset += h - this->height;
    }
    Container::resize(w, h);
}

void Box::push_start(Widget * widget, int x_spacing, int y_spacing, int gravity)
{
    offsets[widget] = {true, x_spacing, y_spacing};
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
    offsets[widget] = {false, x_spacing, y_spacing};
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

void Box::remove(Widget* widget)
{
    uint32_t offset = 0;
    auto it = offsets.find(widget);

    Container::remove(widget);

    if (it == offsets.end()){
        return;     // not pushed by push_start / push_back
    }

    if (type == Type::Horizontal){
        if (it->second.start){
            start_offset -= (it->second.x + widget->get_width());
            offsets.erase(it);
            offset = 0;
            for (auto ch: offsets){
                if (ch.second.start){
                    XMoveWindow(display, ch.first->get_window(),
                            offset+ch.second.x, ch.second.y);
                    offset += ch.second.x + ch.first->get_width();
                }
            }
        } else {    // end_offset
            end_offset += (it->second.x + widget->get_width());
            offsets.erase(it);
            offset = width -1;
            for (auto ch: offsets){
                if (!ch.second.start){
                    offset -= ch.second.x + ch.first->get_width();
                    XMoveWindow(display, ch.first->get_window(),
                            offset, ch.second.y);
                }
            }
        }
    } else {
        TINY_LOG("Not implemented yet");
        offsets.erase(it);
    }
}


Popover::Popover(Box::Type type_arg, uint32_t w, uint32_t h):
    Box(type_arg, w, h),
    time_from_unmap(0)
{
    name = "popover";
}


Popover::Popover(uint32_t w, uint32_t h):
    Popover(Box::Type::Vertical, w, h)
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

void Popover::on_focus_out(const XEvent &, void *)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    time_from_unmap = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    unmap();
}

} // namespace tiny
