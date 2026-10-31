#pragma once

#include <string>

#include "widget.h"
#include "container.h"

namespace tiny {

class Button : public Widget {
  public:
    Button(int width, int height);

    ~Button() override;

    void set_events(long mask = 0) override;

    Signal on_click;
    Signal on_enter;
    Signal on_leave;

  protected:
    virtual void on_enter_notify(const XEvent& e, void* data);
    virtual void on_leave_notify(const XEvent& e, void* data);
    virtual void on_button_release(const XEvent& e, void* data);
};

class LabelButton : public Button {
  public:
    LabelButton(int width, int height, const std::string& text);

    ~LabelButton() override;

    void realize(Window parent, int x, int y) override;

    void set_events(long mask = 0) override;

    void set_text(const std::string& text);

    std::string get_text() const { return text; }

  protected:
    void on_enter_notify(const XEvent& e, void* data) override;
    void on_leave_notify(const XEvent& e, void* data) override;
    virtual void on_expose(const XEvent& e, void* data);

    std::string text;
};

class MenuButton : public LabelButton {
  public:
    MenuButton(int width, int height, const std::string& text);

    ~MenuButton() override;

    void set_events(long mask = 0) override;

    void set_popover(Popover* popover);

  private:
    virtual void do_popup(Object* o, const XEvent& e, void* data);

    Popover* popover = nullptr;
};

} // namespace tiny
