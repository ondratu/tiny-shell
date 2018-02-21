#include <X11/Xutil.h>

#include <assert.h>

#include "wm_widget.h"


void WMVirtual::connect(int event_type, event_signal_t signal, void * data)
{
    connect_window(event_type, window, signal, data);
}

void WMVirtual::disconnect(int event_type)
{
    disconnect_window(event_type, window);
}

void WMVirtual::map()
{
    if (window){
        XMapWindow(display, window);
        is_maped = true;
    }
}

void WMVirtual::unmap()
{
    if (window && is_maped){
        XUnmapWindow(display, window);
        is_maped = false;
    }
}


WMWidget::WMWidget(
        Display * display, Window parent,
        int x, int y, unsigned int width, unsigned int height,
        unsigned int border, unsigned long border_color,
        unsigned long background):
    WMVirtual(display, parent)
{
    window = XCreateSimpleWindow(
        display, parent,
        x, y, width, height,
        border, border_color, background);
}

WMWidget::~WMWidget(){
    unmap();
    XDestroyWindow(display, window);
}


WMInputOnly::WMInputOnly(
        Display * display, Window parent,
        int x, int y, unsigned int width, unsigned int height):
    WMVirtual(display, parent)
{
    assert(DefaultRootWindow(display) != parent);

    window = XCreateWindow(
        display, parent,
        x, y, width, height, 0,
        CopyFromParent, InputOnly, CopyFromParent,
        CopyFromParent, nullptr);
}

WMInputOnly::~WMInputOnly(){
    unmap();
    XDestroyWindow(display, window);
}

WMTransparent::WMTransparent(
        Display * display, Window parent,
        int x, int y, uint32_t width, uint32_t height,
        uint32_t border, uint64_t border_color, uint64_t transparent):
    WMVirtual(display, parent)
{
    // TODO: check to support transparent
    XVisualInfo vinfo;
    XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

    XSetWindowAttributes attr;
    attr.colormap = XCreateColormap(display, parent, vinfo.visual, AllocNone);
    attr.border_pixel = border_color;
    attr.background_pixel = transparent;

    window = XCreateWindow(
        display, parent,
        x, y, width, height, border,
        vinfo.depth, InputOutput, vinfo.visual,
        CWColormap | CWBorderPixel | CWBackPixel, &attr);
}

WMTransparent::~WMTransparent(){
    unmap();
    XDestroyWindow(display, window);
}

bool WMTransparent::is_supported(Display * display)
{
    // TODO
    return false;
}
