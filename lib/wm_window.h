#pragma once

#include <set>

#include "containers.h"
#include "wm_header.h"
#include "wm_buttons.h"
#include "wm_edge.h"

namespace wm {

class Window: public tiny::Container {
  public:
    Window(::Window child, uint32_t width, uint32_t height);

    ~Window();

    static Window * create(::Window parent, ::Window child);

    virtual void realize(::Window parent, int x, int y);

    virtual void set_events(long mask=0);

    virtual void map_all();

    inline bool get_minimized() const
    { return is_minimized; }

    inline ::Window get_child() const
    { return child; }

    void set_focus();

    void close();

    // XXX: could be named iconify....
    void minimize();

    void restore(int x=0, int y=0);

    void maximize();

    void update_protocols();

    void update_properties();

    char * get_net_wm_name();

    /* signal handlers */
    void on_close_click(tiny::Object *o, const XEvent &e, void * data);

    void on_minimize_click(tiny::Object *o, const XEvent &e, void * data);

    void on_maximize_click(tiny::Object *o, const XEvent &e, void * data);

    void on_resize(tiny::Object *o, const XEvent &e, void * data);

    void on_move_resize_begin(tiny::Object *o, const XEvent &e, void * data);

    void on_move_resize_motion(tiny::Object *o, const XEvent &e, void * data);

    void on_window_drag_begin(tiny::Object *o, const XEvent &e, void * data);

    void on_window_drag_motion(tiny::Object *o, const XEvent &e, void * data);

    tiny::Signal on_focus;
    tiny::Signal on_drag_begin;
    tiny::Signal on_drag_motion;
  protected:
    /* event handlers */
    void on_button_press(const XEvent &e, void *data);

    void on_button_release(const XEvent &e, void *data);

    void on_motion_notify(const XEvent &e, void *data);

    void on_focus_in(const XEvent &e, void *data);

    void on_focus_out(const XEvent &e, void *data);

    void on_property_notify(const XEvent &e, void *data);

    bool is_minimized = false;
    bool is_maximize = false;
    bool is_resizable = true;
    bool _net_wm_name = false;

    uint32_t state_width;
    uint32_t state_height;
    int state_x;
    int state_y;

    ::Window child;
    XSizeHints * hints;
    std::set<Atom> protocols;
    std::set<Atom> properties;

    XEvent start_event;                 // state before moving/resizing
    XWindowAttributes start_attrs;

    Header header;
    CloseButton cls_btn;
    MinimizeButton min_btn;
    MaximizeButton max_btn;
    BackWindow shadow;
};


} // namespace wm
