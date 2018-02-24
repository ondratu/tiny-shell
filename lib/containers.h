#pragma once

#include <list>
#include <memory>

#include "widget.h"
#include "theme.h"

#define AutoGravity -1

namespace tiny {

class Container: public Widget, public ContainerInterface {
  public:
    Container(uint32_t width, uint32_t height,
            uint32_t border=WIDGET_BORDER,
            uint32_t border_color=WIDGET_BORDER_COLOR,
            uint32_t background=WIDGET_BACKGROUND);

    virtual ~Container();

    virtual void map_all();

    virtual void set_events();

    virtual void add(Widget * widget, int x=0, int y=0,
            int gravity=NorthWestGravity);
};


class Box: public Container {
  public:
    enum class Type {Horizontal, Vertical};

    Box(Type type, uint32_t width, uint32_t height,
        uint32_t border=WIDGET_BORDER,
        uint32_t border_color=WIDGET_BORDER_COLOR,
        uint32_t background=WIDGET_BACKGROUND);

    virtual ~Box();

    // TODO: on resize correct end_offset!

    inline Type get_type() const
    { return type; }

    virtual void push_start(Widget * widget, int x_spacing=0, int y_spacing=0,
            int gravity=AutoGravity);

    virtual void push_back(Widget * widget, int x_spacing=0, int y_spacing=0,
            int gravity=AutoGravity);

  protected:
    Type type;

  private:
    uint32_t start_offset;
    uint32_t end_offset;
};


} // namespace tiny
