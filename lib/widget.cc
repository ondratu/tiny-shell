#include <X11/Xutil.h>

#include "widget.h"
#include "display.h"

namespace tiny {

Widget::Widget(Type type, uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Object(), type(type), display(get_display()), parent(0), window(0),
    event_mask(0), width(width), height(height), border(border),
    border_color(border_color), background(background),
    is_maped(false), is_realized(false)
{
    name = "widget";
}

Widget::Widget(uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Object(), type(Type::Normal), display(get_display()), parent(0), window(0),
    event_mask(0), width(width), height(height), border(border),
    border_color(border_color), background(background),
    is_maped(false), is_realized(false)
{
    name = "widget";
}

Widget::~Widget(){
    unmap();

    if (event_done){
        disconnect(ConfigureNotify);
    }
    if (window) {
        XSync(display, false);      // Wait for all Ungrab call
        XDestroyWindow(display, window);
    }
}

void Widget::set_events(long mask){
    event_mask |= StructureNotifyMask|mask;
    XSelectInput(display, window, event_mask);

    connect(ConfigureNotify,
            static_cast<tiny::event_signal_t>(&Widget::on_configure_notify));
    event_done = true;
}

void Widget::realize(Window parent, int x, int y)
{
    if (is_realized){
        return error("tiny::Widget is realized yet!");
    }
    this->parent = parent;

    switch(type) {
        case (Type::Normal):
            window = XCreateSimpleWindow(
                    display, parent,
                    x, y, width, height,
                    border, border_color, background);
            break;
        case (Type::Input):
            // TODO: parent must bew root or InputOnly
            window = XCreateWindow(
                    display, parent,
                    x, y, width, height, 0,
                    CopyFromParent, InputOnly, CopyFromParent,
                    CopyFromParent, nullptr);
            break;
        case (Type::Transparent):
            XVisualInfo vinfo;
            XMatchVisualInfo(display, XDefaultScreen(display), 32, TrueColor,
                    &vinfo);

            XSetWindowAttributes attr;
            attr.colormap = XCreateColormap(display, parent, vinfo.visual, AllocNone);
            attr.border_pixel = border_color;
            attr.background_pixel = background;

            window = XCreateWindow(
                    display, parent,
                    x, y, width, height, border,
                    vinfo.depth, InputOutput, vinfo.visual,
                    CWColormap | CWBorderPixel | CWBackPixel, &attr);
            break;
    }

    is_realized = true;
    set_events();
}

void Widget::map()
{
    if (!is_realized){
        return error("tiny::Widget is not realized");
    }

    XMapWindow(display, window);
    is_maped = true;
}

void Widget::map_all(){
    map();
}

void Widget::unmap()
{
    if (is_realized && is_maped){
        XUnmapWindow(display, window);
        is_maped = false;
    }
}

void Widget::resize(uint32_t width, uint32_t height)
{
    if (is_realized){
        XResizeWindow(display, window, width, height);
        // width and height are set by on_configure_notify
    } else {
        this->width = width;
        this->height = height;
    }
}

void Widget::connect(uint16_t event_type, event_signal_t signal, void * data)
{
    connect_window(event_type, window, signal, data);
}

void Widget::disconnect(uint16_t event_type)
{
    disconnect_window(event_type, window);
}

void Widget::on_configure_notify(const XEvent &e, void *){
    width = e.xconfigure.width;
    height = e.xconfigure.height;
}


} // namespace tiny
