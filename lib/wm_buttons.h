#pragma once

#include "buttons.h"

namespace wm {

class Button: public tiny::Button {
  public:
    Button();

    virtual ~Button();

    virtual void set_events(long mask=0);

  protected:
    virtual void on_enter_notify(const XEvent &e, void *data);
    virtual void on_leave_notify(const XEvent &e, void *data);
    virtual void on_expose(const XEvent &e, void *data) = 0;
};


class CloseButton: public Button {
  public:
    CloseButton();

    virtual ~CloseButton();

  protected:
    virtual void on_expose(const XEvent &e, void *data);
};


class MaximizeButton: public Button {
  public:
    MaximizeButton();

    virtual ~MaximizeButton();

    void set_restore(bool restore);
  protected:
    virtual void on_expose(const XEvent &e, void *data);

  private:
    bool restore;
};


class MinimizeButton: public Button {
  public:
    MinimizeButton();

    virtual ~MinimizeButton();

  protected:
    virtual void on_expose(const XEvent &e, void *data);
};

}
