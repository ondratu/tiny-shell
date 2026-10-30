#pragma once

#include <list>
#include <memory>

#include "widget.h"
#include "theme.h"

#define AutoGravity -1

namespace tiny {

class Position {
  public:
    enum { Top = 1, Right = 2, Bottom = 4, Left = 8 };

    static uint32_t get_cursor_shape(uint16_t mask);
};

class Container : public Widget {
  public:
    Container(Widget::Type type, int width, int height,
              const char* name = "container");
    Container(int width, int height, const char* name = "container");

    ~Container() override;

    void map_all() override;

    virtual void add(Widget* widget, int x = 0, int y = 0,
                     int gravity = NorthWestGravity);

    virtual void remove(Widget* widget);

  protected:
    std::list<Widget*> children;
};

class Box : public Container {
  public:
    enum class Type { Horizontal, Vertical };

    Box(Type type, int width, int height);

    ~Box() override;

    void resize(int widget, int height) override;

    Type get_type() const { return type; }

    int get_start_offset() const { return start_offset; }

    int get_end_offset() const { return end_offset; }

    virtual void push_start(Widget* widget, int x_spacing = 0,
                            int y_spacing = 0, int gravity = AutoGravity);

    virtual void push_back(Widget* widget, int x_spacing = 0, int y_spacing = 0,
                           int gravity = AutoGravity);

    void remove(Widget* widget) override;

  protected:
    struct Offset {
        bool start;
        int x;
        int y;
    };

    Type type;

    int start_offset;
    int end_offset;
    std::map<Widget*, Offset> offsets;
};

class Popover : public Box {
  public:
    Popover(Box::Type type, int width, int height);
    Popover(int width, int height);

    ~Popover() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    void set_events(long mask = 0) override;

    void popup(int x, int y);
    void popup(const XButtonEvent& be);

  protected:
    void on_focus_out(const XEvent& e, void*);

  private:
    bool has_focus_inside() const;

    long int time_from_unmap;
};

} // namespace tiny
