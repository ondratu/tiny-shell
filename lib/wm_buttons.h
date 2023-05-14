#pragma once

#include "buttons.h"

namespace wm {

class Button: public tiny::Button {
  public:
    Button();

    virtual ~Button();

    virtual void set_events(long mask=0);

    inline virtual const tiny::Style& get_style() const
    { return tiny::theme.wm_button; }

  protected:
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
