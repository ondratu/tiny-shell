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
    uint32_t width;
    uint32_t height;
};

class Widget: public Object {
  public:
    enum class Type {Normal, Transparent, Input};

    Widget(Type type, uint32_t width, uint32_t height);

    Widget(uint32_t width, uint32_t height);

    virtual ~Widget();

    virtual void realize(Window parent, int x, int y);

    inline const Window get_window() const
    { return window; }

    inline const Window get_parent_window() const
    { return parent; }

    inline virtual uint32_t get_border() const
    { return tiny::theme.widget.border_width; }

    inline virtual uint32_t get_padding() const
    { return tiny::theme.widget.padding_width; }

    inline uint32_t get_width() const
    { return width; }

    inline uint32_t get_height() const
    { return height; }

    inline bool get_maped() const
    { return is_maped; }

    inline bool get_realized() const
    { return is_realized; }

    virtual void set_events(long mask=0);

    virtual void map();

    virtual void map_all();

    virtual void unmap();

    // Generate ResizeRequest and Expose when Expose when size is smaller
    virtual void resize(uint32_t width, uint32_t height);

  protected:
    void connect(uint16_t event_type, event_signal_t signal,
            void * data = nullptr);

    void disconnect(uint16_t event_type);

    virtual void on_configure_notify(const XEvent &e, void *);

    uint8_t get_theme_state() const;

    Type type;
    Display &display;
    Window parent;
    Window window;
    const char* name;

    long event_mask;
    bool event_done;
    uint32_t width;
    uint32_t height;

    bool is_maped;
    bool is_realized;

    bool is_disabled = false;
    bool is_selected = false;   //!< is selected
    bool is_hover = false;      //!< is mouse over
    bool is_focus = false;      //!< is keyboard cursor in / on
    bool is_pressed = false;    //!< is pressed (buttons)
    bool is_dragged = false;    //!< is dragged
};

} // namespace tiny
