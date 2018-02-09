#pragma once

#include "wm_widget.h"

class WMTitle : public WMWidget {
  public:
    typedef void (*on_event_t)(WMTitle &title, const XEvent &e, void * data);

    WMTitle(Display * display, Window parent,
            int x, int y, int width, int height);

    ~WMTitle();

    void connect(const std::string &signal, on_event_t handler, void * data);

    static void on_mouse_press(const XEvent &e, void* _this, void* data);
    static void on_mouse_release(const XEvent &e, void* _this, void* data);
    static void on_mouse_motion(const XEvent &e, void* _this, void* data);
    static void on_expose(const XEvent &e, void* _this, void*data);

  private:
    on_event_t on_mouse_press_handler;
    on_event_t on_mouse_release_handler;
    on_event_t on_mouse_motion_handler;
};

