#pragma once

#include <string>

#include "widget.h"
#include "containers.h"

namespace tiny {

class Button: public Widget {
  public:
    Button(uint32_t width, uint32_t height);

    virtual ~Button();

    virtual void set_events(long mask=0);

    Signal on_click;
    Signal on_enter;
    Signal on_leave;

  protected:
    virtual void on_enter_notify(const XEvent &e, void *data);
    virtual void on_leave_notify(const XEvent &e, void *data);
    virtual void on_button_release(const XEvent &e, void *data);
};


class LabelButton: public Button {
  public:
    LabelButton(uint32_t width, uint32_t height, const std::string &text);

    virtual ~LabelButton();

    virtual void realize(Window parent, int x, int y);

    virtual void set_events(long mask=0);

    void set_text(const std::string &text);

    inline const std::string get_text() const
    { return text; }

  protected:
    virtual void on_enter_notify(const XEvent &e, void *data);
    virtual void on_leave_notify(const XEvent &e, void *data);
    virtual void on_expose(const XEvent &e, void *data);

    std::string text;
};


class MenuButton: public LabelButton {
  public:
    MenuButton(uint32_t width, uint32_t height, const std::string &text);

    ~MenuButton();

    virtual void set_events(long mask=0);

    void set_popover(Popover *popover);

  private:
    virtual void do_popup(Object *o, const XEvent &e, void *data);

    Popover *popover;
};

} // namespace
