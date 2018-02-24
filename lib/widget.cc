#include <X11/Xutil.h>

#include <assert.h>
#include <stdexcept>

#include "widget.h"


namespace tiny {


Virtual::Virtual(uint32_t width, uint32_t height):
    Object(), display(nullptr), parent(0), window(0),
    width(width), height(height), is_maped(false), is_realized(false)
{}

Virtual::~Virtual(){
    unmap();
}

void Virtual::connect(uint16_t event_type, event_signal_t signal, void * data)
{
    connect_window(event_type, window, signal, data);
}

void Virtual::disconnect(uint16_t event_type)
{
    disconnect_window(event_type, window);
}

void Virtual::realize(Display * display, Window parent)
{
    if (!window){
        throw std::runtime_error("Window is not created.");
    }
    this->display = display;
    this->parent = parent;
    is_realized = true;
    set_events();
}

void Virtual::map()
{
    if (is_realized){
        XMapWindow(display, window);
        is_maped = true;
    } else {
        throw std::runtime_error("Window is not realized.");
    }
}

void Virtual::map_all(){
    map();
}

void Virtual::unmap()
{
    if (is_realized && is_maped){
        XUnmapWindow(display, window);
        is_maped = false;
    }
}



Widget::Widget(uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color,
        uint32_t background):
    Virtual(width, height),
        border(border), border_color(border_color), background(background)
{}

Widget::~Widget(){
    XDestroyWindow(display, window);
}

void Widget::realize(Display * display, Window parent, int x, int y)
{
    window = XCreateSimpleWindow(
        display, parent,
        x, y, width, height,
        border, border_color, background);
    Virtual::realize(display, parent);
}


InputWidget::InputWidget( uint32_t width, uint32_t height):
    Virtual(width, height)
{}

InputWidget::~InputWidget(){
    XDestroyWindow(display, window);
}

void InputWidget::realize(Display * display, Window parent, int x, int y)
{
    // TODO: parent must bew root or InputOnly
    window = XCreateWindow(
        display, parent,
        x, y, width, height, 0,
        CopyFromParent, InputOnly, CopyFromParent,
        CopyFromParent, nullptr);
    Virtual::realize(display, parent);
}


Transparent::Transparent(
        uint32_t width, uint32_t height,
        uint32_t border, uint64_t border_color, uint64_t background):
    Virtual(width, height), border(border), border_color(border_color),
    background(background)
{}

Transparent::~Transparent(){
    XDestroyWindow(display, window);
}

void Transparent::realize(Display * display, Window parent, int x, int y)
{
    // TODO: check to support transparent
    XVisualInfo vinfo;
    XMatchVisualInfo(display, XDefaultScreen(display), 32, TrueColor, &vinfo);

    XSetWindowAttributes attr;
    attr.colormap = XCreateColormap(display, parent, vinfo.visual, AllocNone);
    attr.border_pixel = border_color;
    attr.background_pixel = background;

    window = XCreateWindow(
        display, parent,
        x, y, width, height, border,
        vinfo.depth, InputOutput, vinfo.visual,
        CWColormap | CWBorderPixel | CWBackPixel, &attr);

    Virtual::realize(display, parent);
}

bool Transparent::is_supported(Display * display)
{
    // TODO
    return false;
}



ContainerInterface::ContainerInterface()
{}

ContainerInterface::~ContainerInterface()
{}

void ContainerInterface::add(Widget * widget){
    children.push_back(widget);
}


} // namespace tiny
