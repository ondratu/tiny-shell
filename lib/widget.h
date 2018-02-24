#pragma once

#include <X11/Xft/Xft.h>

#include <list>

#include "object.h"
#include "theme.h"

namespace tiny {

class Virtual: public Object {
  public:
    Virtual(uint32_t width, uint32_t height);

    virtual ~Virtual();

    virtual void realize(Display * display, Window parent);

    inline const Window get_window() const
    { return window; }

    inline const Window get_parent_window() const
    { return parent; }

    inline uint32_t get_width() const
    { return width; }

    inline uint32_t get_height() const
    { return height; }

    virtual void set_events() = 0;

    virtual void map();

    virtual void map_all();

    virtual void unmap();

  protected:
    void connect(uint16_t event_type, event_signal_t signal,
            void * data = nullptr);

    void disconnect(uint16_t event_type);


    Display * display;
    Window parent;
    Window window;

    uint32_t width;
    uint32_t height;

    bool is_maped;
    bool is_realized;
};


class Widget: public Virtual {
  public:
    Widget(uint32_t width, uint32_t height,
            uint32_t border=WIDGET_BORDER,
            uint32_t border_color=WIDGET_BORDER_COLOR,
            uint32_t background=WIDGET_BACKGROUND);

    virtual ~Widget();

    virtual void realize(Display * display, Window parent, int x, int y);


  protected:
    uint32_t border;
    uint32_t border_color;
    uint32_t background;
};


class InputWidget: public Virtual {
  public:
    InputWidget(uint32_t width, uint32_t height);

    virtual ~InputWidget();

    virtual void realize(Display * display, Window parent, int x, int y);
};


class Transparent: public Virtual {
  public:
    Transparent(
            uint32_t width, uint32_t height,
            uint32_t border=WIDGET_BORDER,
            uint64_t border_color=WIDGET_BORDER_COLOR,
            uint64_t transparent=WIDGET_BACKGROUND);

    virtual ~Transparent();

    virtual void realize(Display * display, Window parent, int x, int y);

    static bool is_supported(Display *display);

  protected:
    uint32_t border;
    uint64_t border_color;
    uint64_t background;
};


class ContainerInterface {
  public:
    ContainerInterface();

    virtual ~ContainerInterface();

    virtual void add(Widget * widget);

  protected:
    std::list<Widget*> children;
};


} // namespace tiny
