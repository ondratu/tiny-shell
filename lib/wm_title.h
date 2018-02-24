#pragma once

#include <X11/Xft/Xft.h>

#include <string>

#include "wm_widget.h"

class WMTitle: public WMWidget {
  public:
    WMTitle(Display * display, Window parent,
            int x, int y, int width, int height);

    virtual ~WMTitle();

    virtual void set_events();

    inline const std::string get_text() const
    { return text; }

    void set_text(const std::string &text);

    Signal on_drag_begin;
    Signal on_drag_end;
    Signal on_drag_motion;

  private:
    void on_button_press(const XEvent &e, void *data);
    void on_button_release(const XEvent &e, void *data);
    void on_motion_notify(const XEvent &e, void *data);
    void on_expose(const XEvent &e, void *data);

    std::string text;
    // GC gc;
    // XFontStruct * font;
    XftFont *xft_font;      // internally reference-counted
};
