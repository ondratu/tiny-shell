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

    void set_events(long mask = 0) override;

    void set_text(const std::string& text);

    const std::string& get_text() const { return text; }

    //! The width of the text in the font of the widgets
    int text_width() const;

    //! The text starts `padding` from the left (it is centered by default)
    void set_text_padding(int padding) { text_padding = padding; }

    //! The width of the border instead of the one of the style (an item of a
    //! menu has none); set it before the button is realized
    void set_border(int border_width) { border = border_width; }

    int get_border() const override
    {
        return border >= 0 ? border : Button::get_border();
    }

  protected:
    void on_enter_notify(const XEvent& e, void* data) override;
    void on_leave_notify(const XEvent& e, void* data) override;
    virtual void on_expose(const XEvent& e, void* data);

    std::string text;
    int text_padding = -1; //!< -1: centered
    int border = -1;       //!< -1: the border of the style
};

class MenuButton : public LabelButton {
  public:
    MenuButton(int width, int height, const std::string& text);

    ~MenuButton() override;

    void set_events(long mask = 0) override;

    void set_popover(Popover* popover);

    //! Open the popover with its left edge at the left edge of the button,
    //! not centered under it
    void set_popover_left(bool left) { popover_left = left; }

  private:
    virtual void do_popup(Object* o, const XEvent& e, void* data);

    Popover* popover = nullptr;
    bool popover_left = false;
};

} // namespace tiny
