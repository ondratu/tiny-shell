#pragma once

#include <map>
#include <list>

#include "object.h"
#include "wm_window.h"

namespace wm {

class Manager: public tiny::Object {
  public:
    Manager();

    ~Manager();

    void main_loop();

  private:
    void set_events();

    void activate_next_window();

    void activate_prev_window();

    /* direct event handlers */
    void on_key_press(const XKeyEvent &e);

    void on_key_release(const XKeyEvent &e);

    void on_unmap_notify(const XUnmapEvent &e);

    void on_map_request(const XMapRequestEvent &e);

    void on_configure_request(const XConfigureRequestEvent &e);

    Display * display;
    ::Window root;

    std::map<::Window, Window*> wm_windows;
    std::list<Window*> wm_tops;
};

} // namespace wm
