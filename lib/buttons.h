#pragma once

#include "widget.h"
#include "containers.h"

namespace tiny {

class Button: public Widget {
  public:
    Button(uint32_t width, uint32_t height,
        uint32_t border=WIDGET_BORDER,
        uint32_t border_color=WIDGET_BORDER_COLOR,
        uint32_t background=WIDGET_BACKGROUND);
    // TODO: use style instead of each colors...

    virtual ~Button();

    virtual void set_events(long mask=0);

    inline bool get_active()
    { return is_active; }

    Signal on_click;

  protected:
    virtual void on_enter_notify(const XEvent &e, void *data);
    virtual void on_leave_notify(const XEvent &e, void *data);
    virtual void on_button_release(const XEvent &e, void *data);

    bool is_active;
};


class LabelButton: public Button {
  public:
    LabelButton(uint32_t width, uint32_t height, const std::string &text,
        const std::string &font=WIDGET_XFT_FONT,
        uint32_t border=WIDGET_BORDER,
        uint32_t border_color=WIDGET_BORDER_COLOR,
        uint32_t background=WIDGET_BACKGROUND);

    virtual ~LabelButton();

    virtual void realize(Display * display, Window parent, int x, int y);

    virtual void set_events(long mask=0);

    void set_text(const std::string &text);

    inline const std::string get_text() const
    { return text; }

    void set_text_colors(
            const std::string &normal_color,
            const std::string &active_color);

  protected:
    virtual void on_enter_notify(const XEvent &e, void *data);
    virtual void on_leave_notify(const XEvent &e, void *data);
    virtual void on_expose(const XEvent &e, void *data);

    std::string text;
    std::string font_name;
    std::string normal_color;
    std::string active_color;

  private:
    XftFont *font;
    const int screen;       // XXX: this is zero for now
};


class MenuButton: public LabelButton {
  public:
    MenuButton(uint32_t width, uint32_t height, const std::string &text,
            const std::string &font=WIDGET_XFT_FONT,
            uint32_t border=WIDGET_BORDER,
            uint32_t border_color=WIDGET_BORDER_COLOR,
            uint32_t background=WIDGET_BACKGROUND);

    ~MenuButton();

    virtual void set_events(long mask=0);

    void set_popover(Popover *popover);

  private:
    virtual void do_popup(Object *o, const XEvent &e, void *data);

    Popover *popover;
};

} // namespace
