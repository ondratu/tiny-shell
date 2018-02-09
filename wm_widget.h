#pragma once

#include "wm_handlers.h"

class WMWidget {
  public:
    typedef void (*on_event_t)(WMWidget &w, const XEvent &e, void * data);

    WMWidget(Display * display, Window parent,
             int x, int y, unsigned int width, unsigned int height,
             unsigned int border=0, unsigned long border_color=0x0,
             unsigned long background=0x0);
    ~WMWidget();

    Window parent;
    Window window;

  protected:
    Display * display;
};

