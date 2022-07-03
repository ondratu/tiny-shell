#pragma once

#include <map>
#include <list>

#include "object.h"
#include "wm_window.h"
#include "wm_panel.h"
#include "wm_run.h"

namespace wm {

class Manager: public tiny::Object {
  public:
    Manager(Display* display, ::Window root);

    ~Manager();

    void main_loop();

  private:
    void set_events();

    void activate_next_window();

    void activate_prev_window();

    /* signal event handlers */
    void on_window_focus(tiny::Object *o, const XEvent &e, void *data);
    void on_logout(tiny::Object *o, const XEvent &e, void *data);

    /* direct event handlers */
    void on_key_press(const XKeyEvent &xkey);

    void on_key_release(const XKeyEvent &xkey);

    void on_unmap_notify(const XUnmapEvent &e);

    void on_map_request(const XMapRequestEvent &e);

    void on_configure_request(const XConfigureRequestEvent &e);

    ::Display* display;
    ::Window root;

    Panel wm_panel;
    RunDialog wm_run;

    Window *active;

    std::map<::Window, Window*> wm_windows;
    std::list<Window*> wm_tops;
    bool key_done;
    bool running;
};

} // namespace wm
