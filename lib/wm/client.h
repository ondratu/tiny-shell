#pragma once

#include <memory>

#include "container.h"
#include "wm/header.h"
#include "wm/window_button.h"
#include "wm/edge.h"
#include "wm/size_label.h"
#include "wm/window.h"

namespace tiny::wm {

class Client : public tiny::Container, public Window {

  public:
    Client(::Window child, ::Window root, int width, int height,
           unsigned long functions, unsigned long decorations);

    ~Client() override;

    static Window* create(::Window root, ::Window child,
                          const XWindowAttributes& attrs,
                          unsigned long functions, unsigned long decorations);

    void realize(::Window parent, int x, int y) override;

    void set_events(long mask = 0) override;

    void map_all() override;

    void set_focus() override;

    //! Minimize (Iconify) window
    void minimize() override;

    void restore(int x = 0, int y = 0) override;

    void maximize() override;

    /* signal handlers */
    void on_close_click(tiny::Object* o, const XEvent& e, void* data);

    void on_minimize_click(tiny::Object* o, const XEvent& e, void* data);

    void on_maximize_click(tiny::Object* o, const XEvent& e, void* data);

    void on_resize(tiny::Object* o, const XEvent& e, void* data);

    void on_move_resize_begin(tiny::Object* o, const XEvent& e, void* data);

    void on_move_resize_motion(tiny::Object* o, const XEvent& e, void* data);

    void on_move_resize_end(tiny::Object* o, const XEvent& e, void* data);

    void on_window_drag_begin(tiny::Object* o, const XEvent& e,
                              void* data) override;

    void on_window_drag_motion(tiny::Object* o, const XEvent& e,
                               void* data) override;

  protected:
    /* event handlers */

    void on_focus_in(const XEvent& e, void* data) override;

    void on_focus_out(const XEvent& e, void* data) override;

    void on_property_notify(const XEvent& e, void* data) override;

    //! Show the size of the client in units (e.g. columns and rows)
    void show_size_units(int client_width, int client_height);

    unsigned long decorations = 0;

    Header header;
    CloseButton cls_btn;
    MinimizeButton min_btn;
    MaximizeButton max_btn;
    BackWindow shadow;
    std::unique_ptr<SizeLabel> size_label; //!< only while the size is changed
};

} // namespace tiny::wm
