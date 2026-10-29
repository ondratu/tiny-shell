#pragma once

#include "buttons.h"

namespace wm {

class Button : public tiny::Button {
  public:
    Button();

    ~Button() override;

    void set_events(long mask = 0) override;

    const tiny::Style& get_style() const override
    {
        return tiny::theme.wm_button;
    }

  protected:
    void on_enter_notify(const XEvent& e, void* data) override;
    void on_leave_notify(const XEvent& e, void* data) override;
    virtual void on_expose(const XEvent& e, void* data) = 0;
};

class CloseButton : public Button {
  public:
    CloseButton();

    ~CloseButton() override;

  protected:
    void on_expose(const XEvent& e, void* data) override;
};

class MaximizeButton : public Button {
  public:
    MaximizeButton();

    ~MaximizeButton() override;

    void set_restore(bool restore);

  protected:
    void on_expose(const XEvent& e, void* data) override;

  private:
    bool restore;
};

class MinimizeButton : public Button {
  public:
    MinimizeButton();

    ~MinimizeButton() override;

  protected:
    void on_expose(const XEvent& e, void* data) override;
};

} // namespace wm
