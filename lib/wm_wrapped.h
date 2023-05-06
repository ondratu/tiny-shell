#pragma once

#include "containers.h"
#include "wm_header.h"
#include "wm_buttons.h"
#include "wm_edge.h"
#include "wm_window.h"

namespace wm {

class Wrapped: public tiny::Container, public Window {

  public:
    Wrapped(::Window child, ::Window root, uint32_t width, uint32_t height,
            unsigned long functions, unsigned long decorations);

    ~Wrapped();

    static Window * create(::Window parent, ::Window child,
            const XWindowAttributes& attrs,
            unsigned long functions, unsigned long decorations);

    virtual void realize(::Window parent, int x, int y) override;

    virtual void set_events(long mask=0) override;

    virtual void map_all() override;

    virtual void set_focus() override;

    // XXX: could be named iconify....
    void minimize();

    virtual void restore(int x=0, int y=0) override;

    virtual void maximize() override;

    char * get_net_wm_name();

    /* signal handlers */
    void on_close_click(tiny::Object *o, const XEvent &e, void * data);

    void on_minimize_click(tiny::Object *o, const XEvent &e, void * data);

    void on_maximize_click(tiny::Object *o, const XEvent &e, void * data);

    void on_resize(tiny::Object *o, const XEvent &e, void * data);

    void on_move_resize_begin(tiny::Object *o, const XEvent &e, void * data);

    void on_move_resize_motion(tiny::Object *o, const XEvent &e, void * data);

    virtual void on_window_drag_begin(tiny::Object *o, const XEvent &e,
            void * data) override;

    virtual void on_window_drag_motion(tiny::Object *o, const XEvent &e,
            void * data) override;

  protected:
    /* event handlers */

    void on_focus_in(const XEvent &e, void *data);

    void on_focus_out(const XEvent &e, void *data);

    virtual void on_property_notify(const XEvent &e, void *data) override;

    bool _net_wm_name = false;

    unsigned long decorations = 0;

    Header header;
    CloseButton cls_btn;
    MinimizeButton min_btn;
    MaximizeButton max_btn;
    BackWindow shadow;
};

} // namespace
