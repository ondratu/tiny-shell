#pragma once

#include <string>

#include "wm/shell_theme.h"
#include "wm/symbolic_icons.h"
#include "button.h"

namespace tiny::wm {

class WindowButton : public tiny::Button {
  public:
    WindowButton();

    ~WindowButton() override;

    void set_events(long mask = 0) override;

    const tiny::Style& get_style() const override { return shell_theme.button; }

    //! The icons to draw the symbol with (`window-close`...); without them,
    //! or without the file, the symbol is drawn with lines. They must live
    //! longer than the button.
    void set_icons(SymbolicIcons* icons_arg) { icons = icons_arg; }

  protected:
    //! Paint the background and the icon; false (and the background only)
    //! when there is no icon to draw
    bool draw_icon(const std::string& icon_name);

    void on_enter_notify(const XEvent& e, void* data) override;
    void on_leave_notify(const XEvent& e, void* data) override;
    virtual void on_expose(const XEvent& e, void* data) = 0;

  private:
    SymbolicIcons* icons = nullptr;
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
