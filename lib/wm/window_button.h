#pragma once

#include "button.h"

namespace tiny::wm {

class WindowButton : public tiny::Button {
  public:
    WindowButton();

    ~WindowButton() override;

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

class CloseButton : public WindowButton {
  public:
    CloseButton();

    ~CloseButton() override;

  protected:
    void on_expose(const XEvent& e, void* data) override;
};

class MaximizeButton : public WindowButton {
  public:
    MaximizeButton();

    ~MaximizeButton() override;

    void set_restore(bool restore);

  protected:
    void on_expose(const XEvent& e, void* data) override;

  private:
    bool restore;
};

class MinimizeButton : public WindowButton {
  public:
    MinimizeButton();

    ~MinimizeButton() override;

  protected:
    void on_expose(const XEvent& e, void* data) override;
};

} // namespace tiny::wm
