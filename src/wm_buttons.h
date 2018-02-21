#pragma once

#include "wm_widget.h"

class WMButton: public WMWidget {
  public:
    WMButton(Display *display, Window parent,
             int x, int y, uint32_t width, uint32_t height);

    virtual ~WMButton();

    virtual void set_events();

    Signal on_click;

  protected:
    virtual void on_enter_notify(const XEvent &e, void *data);
    virtual void on_leave_notify(const XEvent &e, void *data);
    virtual void on_button_release(const XEvent &e, void *data);
};

class WMCloseButton: public WMButton {
  public:
    WMCloseButton(Display * display, Window parent,
                  int x, int y, uint32_t width, uint32_t height);

    ~WMCloseButton();

    virtual void set_events();

  protected:
    virtual void on_expose(const XEvent &e, void *data);
};
