#pragma once

#include "wm_object.h"

#include <inttypes.h>

class WMVirtual: public WMObject {
  public:
    WMVirtual(Display * display, Window parent):
        WMObject(), parent(parent), display(display)
    {}

    virtual ~WMVirtual()
    {}

    const Window get_window() const {
        return window;
    }

    const Window get_parent_window() const {
        return parent;
    }

    virtual void map();

    virtual void unmap();

  protected:
    void connect(int event_type, event_signal_t signal, void * data = nullptr);

    void disconnect(int event_type);

    Display * display;
    Window parent;
    Window window = 0;
    bool is_maped = false;
};


class WMWidget: public WMVirtual {
  public:
    WMWidget(Display * display, Window parent,
             int x, int y, unsigned int width, unsigned int height,
             unsigned int border=0, unsigned long border_color=0x0,
             unsigned long background=0x0);

    virtual ~WMWidget();

    virtual void set_events() = 0;
};


class WMInputOnly: public WMVirtual {
  public:
    WMInputOnly(Display * display, Window parent,
             int x, int y, unsigned int width, unsigned int height);

    virtual ~WMInputOnly();

    virtual void set_events() = 0;
};


class WMTransparent: public WMVirtual {
  public:
    WMTransparent(Display * display, Window parent,
             int x, int y, uint32_t width, uint32_t height,
             uint32_t border=0, uint64_t border_color=0x0,
             uint64_t transparent=0x0);

    virtual ~WMTransparent();

    virtual void set_events() = 0;

    static bool is_supported(Display *display);
};
