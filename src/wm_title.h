#pragma once

#include <string>

#include "wm_widget.h"

class WMTitle: public WMWidget {
  public:
    WMTitle(Display * display, Window parent,
            int x, int y, int width, int height);

    virtual ~WMTitle();

    virtual void set_events();

    inline const std::string get_title() const
    { return title; }

    void set_title(const std::string &title);

    Signal on_drag_begin;
    Signal on_drag_end;
    Signal on_drag_motion;

  private:
    void on_button_press(const XEvent &e, void *data);
    void on_button_release(const XEvent &e, void *data);
    void on_motion_notify(const XEvent &e, void *data);
    void on_expose(const XEvent &e, void *data);

    std::string title;
};
