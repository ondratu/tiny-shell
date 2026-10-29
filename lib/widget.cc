#include <X11/Xutil.h>

#include <algorithm>

#include "widget.h"
#include "x_util.h"
#include "display.h"

namespace tiny {

Widget::Widget(Type type_arg, int w, int h):

    type(type_arg),
    display(get_display()),
    parent(0),
    window(0),
    event_mask(0),
    event_done(false),
    width(std::max(0, w)),
    height(std::max(0, h)),
    is_maped(false),
    is_realized(false)
{
    name = "widget";
}

Widget::Widget(int w, int h): Widget(Type::Normal, w, h) {}

Widget::~Widget()
{
    Widget::unmap();

    if (event_done) {
        disconnect(ConfigureNotify);
    }
    if (window) {
        XSync(display, False); // Wait for all Ungrab call
        XDestroyWindow(display, window);
    }
}

uint8_t Widget::get_theme_state() const
{
    auto state = static_cast<uint8_t>(Style::State::Normal);
    if (is_selected) {
        state |= static_cast<uint8_t>(Style::State::Selected);
    }
    if (is_hover) {
        state |= static_cast<uint8_t>(Style::State::Hover);
    }
    if (is_focus) {
        state |= static_cast<uint8_t>(Style::State::Focus);
    }
    if (is_pressed) {
        state |= static_cast<uint8_t>(Style::State::Pressed);
    }
    if (is_disabled) {
        state |= static_cast<uint8_t>(Style::State::Disabled);
    }
    return state;
}

void Widget::set_events(long mask)
{
    event_mask |= StructureNotifyMask | mask;
    XSelectInput(display, window, event_mask);

    connect(ConfigureNotify,
            static_cast<tiny::event_signal_t>(&Widget::on_configure_notify));
    event_done = true;
}

void Widget::realize(Window parent_arg, int x, int y)
{
    if (is_realized) {
        return error("tiny::Widget is realized yet!");
    }
    this->parent = parent_arg;
    const uint8_t state = get_theme_state();

    switch (type) {
        case Type::Normal:
            window = XCreateSimpleWindow(
                    display, parent_arg, x, y, x_dim(width), x_dim(height),
                    x_uint(get_border()), get_style().get_br(state),
                    get_style().get_bg(state));
            break;
        case Type::Input:
            // TODO: parent must bew root or InputOnly
            window = XCreateWindow(display, parent_arg, x, y, x_dim(width),
                                   x_dim(height), 0, CopyFromParent, InputOnly,
                                   CopyFromParent, CopyFromParent, nullptr);
            break;
        case Type::Transparent:
            XVisualInfo vinfo;
            if (!XMatchVisualInfo(display, XDefaultScreen(display), 32,
                                  TrueColor, &vinfo)) {
                // no ARGB visual: fall back to an opaque window
                error("tiny::Widget %s: no 32 bit visual, not transparent",
                      name);
                window = XCreateSimpleWindow(
                        display, parent_arg, x, y, x_dim(width), x_dim(height),
                        x_uint(get_border()), get_style().get_br(state),
                        get_style().get_bg(state));
                break;
            }

            XSetWindowAttributes attr;
            attr.colormap = XCreateColormap(display, parent_arg, vinfo.visual,
                                            AllocNone);
            attr.border_pixel = get_style().get_br(state);
            attr.background_pixel = get_style().get_bg(state);

            window = XCreateWindow(display, parent_arg, x, y, x_dim(width),
                                   x_dim(height), x_uint(get_border()),
                                   vinfo.depth, InputOutput, vinfo.visual,
                                   CWColormap | CWBorderPixel | CWBackPixel,
                                   &attr);
            break;
    }

    is_realized = true;
    set_events();
}

void Widget::map()
{
    if (!is_realized) {
        return error("tiny::Widget is not realized");
    }

    XMapWindow(display, window);
    is_maped = true;
}

void Widget::map_all()
{
    map();
}

void Widget::unmap()
{
    if (is_realized && is_maped) {
        XUnmapWindow(display, window);
        is_maped = false;
    }
}

void Widget::resize(int w, int h)
{
    if (is_realized) {
        XResizeWindow(display, window, x_dim(w), x_dim(h));
        // width and height are set by on_configure_notify
    } else {
        this->width = std::max(0, w);
        this->height = std::max(0, h);
    }
}

void Widget::connect(uint16_t event_type, event_signal_t signal, void* data)
{
    connect_window(event_type, window, signal, data);
}

void Widget::disconnect(uint16_t event_type)
{
    disconnect_window(event_type, window);
}

void Widget::on_configure_notify(const XEvent& e, void*)
{
    width = e.xconfigure.width;
    height = e.xconfigure.height;
}

} // namespace tiny
