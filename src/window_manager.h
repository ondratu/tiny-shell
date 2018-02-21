#pragma once
#include <memory>
#include <map>
#include <list>

#include "wm_object.h"
#include "wm_window.h"

class WindowManager: public WMObject {
  public:
    virtual ~WindowManager();

    static std::unique_ptr<WindowManager> create();

    void main_loop();

  private:
    WindowManager(Display* display);

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
    const Window root;

    std::map<Window, WMWindow*> wm_windows;
    std::list<WMWindow*> wm_tops;
};
