#pragma once

#include "containers.h"
#include "wm_header.h"
#include "wm_buttons.h"
#include "wm_edge.h"

namespace wm {

class Window: public tiny::Container {
  public:
    Window(::Window child, uint32_t width, uint32_t height);

    ~Window();

    static Window * create(Display * display, ::Window parent, ::Window child);

    virtual void realize(Display * display, ::Window parent, int x, int y);

    virtual void set_events(long mask=0);

    virtual void map_all();

    inline const bool get_minimized() const
    { return is_minimized; }

    void set_focus();

    void close();

    // XXX: could be named iconify....
    void minimize();

    void restore();

    /* signal handlers */
    void on_close_click(tiny::Object *o, const XEvent &e, void * data);

    void on_minimize_click(tiny::Object *o, const XEvent &e, void * data);

    void on_maximize_click(tiny::Object *o, const XEvent &e, void * data);

    void on_resize(tiny::Object *o, const XEvent &e, void * data);

    void on_move_resize_begin(tiny::Object *o, const XEvent &e, void * data);

    void on_move_resize_motion(tiny::Object *o, const XEvent &e, void * data);

    void on_window_drag_begin(tiny::Object *o, const XEvent &e, void * data);

    void on_window_drag_motion(tiny::Object *o, const XEvent &e, void * data);

  protected:
    /* event handlers */
    void on_button_press(const XEvent &e, void *data);

    void on_focus_in(const XEvent &e, void *data);

    void on_focus_out(const XEvent &e, void *data);

    void on_property_notify(const XEvent &e, void *data);

    bool is_minimized = false;
    bool is_resizable = true;

    ::Window child;
    XSizeHints * hints;

    XEvent start_event;                 // state before moving/resizing
    XWindowAttributes start_attrs;

    Header header;
    CloseButton cls_btn;
    MinimizeButton min_btn;
    MaximizeButton max_btn;
    BackWindow shadow;
};


} // namespace wm
