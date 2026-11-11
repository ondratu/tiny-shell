#pragma once

#include <X11/Xft/Xft.h>

#include <list>

#include "object.h"
#include "display.h"
#include "theme.h"

namespace tiny {

struct Rect {
    int x;
    int y;
    int width;
    int height;
};

class Widget : public Object {
  public:
    enum class Type { Normal, Transparent, Input };

    Widget(Type type, int width, int height);

    Widget(int width, int height);

    ~Widget() override;

    virtual void realize(Window parent, int x, int y);

    Window get_window() const { return window; }

    Window get_parent_window() const { return parent; }

    virtual const Style& get_style() const
    {
        return style_override != nullptr ? *style_override : theme.widget;
    }

    //! Draw with this style instead of the one of the theme (the panel and
    //! the dock have colors of their own); a container gives it to what it
    //! holds, now and later. Null is the style of the theme. The style must
    //! live longer than the widget.
    virtual void set_style_override(const Style* style)
    {
        style_override = style;
    }

    virtual int get_border() const { return get_style().border_width; }

    virtual int get_padding() const { return get_style().padding_width; }

    int get_width() const { return width; }

    int get_height() const { return height; }

    bool get_maped() const { return is_maped; }

    bool get_realized() const { return is_realized; }

    virtual void set_events(long mask = 0);

    virtual void map();

    virtual void map_all();

    virtual void unmap();

    //! Take the colors of the style again (the theme changed its variant) and
    //! repaint; a widget that is not shown does it when it is mapped
    virtual void restyle();

    // Generate ResizeRequest and Expose when Expose when size is smaller
    virtual void resize(int width, int height);

  protected:
    void connect(uint16_t event_type, event_function_t function);

    void disconnect(uint16_t event_type);

    virtual void on_configure_notify(const XEvent& e, void*);

    uint8_t get_theme_state() const;

    Type type;
    Display& display;
    Window parent;
    Window window;
    const char* name;

    long event_mask;
    bool event_done;
    int width;
    int height;

    bool is_maped;
    bool is_realized;

    bool is_disabled = false;
    bool is_selected = false; //!< is selected
    bool is_hover = false;    //!< is mouse over
    bool is_focus = false;    //!< is keyboard cursor in / on
    bool is_pressed = false;  //!< is pressed (buttons)
    bool is_dragged = false;  //!< is dragged

    const Style* style_override = nullptr; //!< see `set_style_override()`

  private:
    //! Give the window the background and the border color of the style
    void apply_colors();

    unsigned colors_generation; //!< of the theme that the window has colors of
};

} // namespace tiny
