#pragma once
#include "wm_widget.h"

class WMCorner: public WMWidget {
  public:
    enum class Type {LeftTop, LeftBottom, RightTop, RightBottom};

    WMCorner(Display *display, Window parent,
             int x, int y, Type type);

    virtual ~WMCorner();

    virtual void set_events();

    inline const Type get_type() const
    { return type; }

    void set_type(Type type);

    Signal on_drag_begin;
    Signal on_drag_end;
    Signal on_drag_motion;

  protected:
    void on_button_press(const XEvent &e, void *data);
    void on_button_release(const XEvent &e, void *data);
    void on_motion_notify(const XEvent &e, void *data);

  private:
    Type type;
};
