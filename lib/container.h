#pragma once

#include <list>
#include <memory>
#include <vector>

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

    // cppcheck-suppress duplInheritedMember
    Type type;

    int start_offset;
    int end_offset;
    std::map<Widget*, Offset> offsets;
};

/**
 * @short An invisible window over the whole screen that sees a click.
 *
 * A popover puts it under itself, so a click anywhere else closes the popover
 * and does not reach the window below.
 */
class ClickCatcher : public Widget {
  public:
    ClickCatcher();
    ~ClickCatcher() override;

    void set_events(long mask = 0) override;

    Signal on_press;
};

class Popover : public Box {
  public:
    Popover(Box::Type type, int width, int height);
    Popover(int width, int height);

    ~Popover() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    void set_events(long mask = 0) override;

    //! Also hides the window that catches the clicks outside
    void unmap() override;

    //! Show the popover; a click outside of it closes it
    void popup(int x, int y);
    void popup(const XButtonEvent& be);

    //! A popover that opens from this one (a submenu): the focus in it does
    //! not close this popover
    void add_nested(const Popover* popover) { nested.push_back(popover); }

  protected:
    void on_focus_out(const XEvent& e, void*);

  private:
    bool has_focus_inside() const;
    bool is_inside(::Window focus) const;
    bool is_nested(::Window focus) const;

    ClickCatcher catcher;
    std::vector<const Popover*> nested;
    long int time_from_unmap;
};

} // namespace tiny
