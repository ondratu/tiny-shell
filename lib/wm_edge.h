#pragma once
#include "containers.h"

namespace wm {

class Edge: public tiny::Widget {
  public:
    Edge(uint32_t width, uint32_t height, uint16_t mask);

    virtual ~Edge();

    virtual void realize(Window parent, int x, int y);

    virtual void set_events(long mask=0);

    inline uint16_t get_mask() const
    { return mask; }

    tiny::Signal on_drag_begin;
    tiny::Signal on_drag_end;
    tiny::Signal on_drag_motion;

  protected:
    void on_button_press(const XEvent &e, void *data);
    void on_button_release(const XEvent &e, void *data);
    void on_motion_notify(const XEvent &e, void *data);

  private:
    uint16_t mask;
};


class BackWindow: public tiny::Container {
  public:

    BackWindow(uint32_t width, uint32_t height);

    virtual ~BackWindow();

    virtual void realize(Window parent, int x, int y);

    virtual void set_events(long mask=0);

    virtual void resize(uint32_t width, uint32_t height);

    virtual void move(int x, int y);

    virtual void move_resize(int x, int y, uint32_t width, uint32_t height);

    tiny::Signal on_move_resize_begin;
    tiny::Signal on_move_resize_motion;

  protected:
    virtual void on_edge_drag_begin(Object *o, const XEvent &e, void *data);
    virtual void on_edge_drag_motion(Object *o, const XEvent &e, void *data);

    virtual void on_configure_notify(const XEvent &e, void *);

  private:
    Edge corner_right_top;         /* corners */
    Edge corner_right_bottom;
    Edge corner_left_top;
    Edge corner_left_bottom;

    Edge edge_top;               /* edges */
    Edge edge_right;
    Edge edge_bottom;
    Edge edge_left;
};

} // namespace wm
