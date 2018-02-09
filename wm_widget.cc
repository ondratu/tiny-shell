#include <X11/Xutil.h>

#include "wm_widget.h"

WMWidget::WMWidget(Display * display, Window parent,
                   int x, int y, unsigned int width, unsigned int height,
                   unsigned int border, unsigned long border_color,
                   unsigned long background):
        window(0), parent(parent), display(display)
{
    window = XCreateSimpleWindow(
        display, parent,
        x, y, width, height,
        border, border_color, background);

    XSelectInput(display, window, ExposureMask);
    XMapWindow(display, window);
}

WMWidget::~WMWidget(){
    if (window) {
        XUnmapWindow(display, window);
        XDestroyWindow(display, window);
    }
}
