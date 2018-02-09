#pragma once

#include "wm_widget.h"

class WMCorner : public WMWidget {
  public:
    enum class Type {LeftTop, LeftBottom, RightTop, RightBottom};

    WMCorner(Display * display, Window parent,
             int x, int y, Type type);

    ~WMCorner();

    void connect(const std::string &signal, on_event_t handler, void * data);

    static void on_mouse_press(const XEvent &e, void* _this, void* data);
    static void on_mouse_release(const XEvent &e, void* _this, void* data);
    static void on_mouse_motion(const XEvent &e, void* _this, void* data);

    Type type;

  private:
    on_event_t on_mouse_press_handler;
    on_event_t on_mouse_release_handler;
    on_event_t on_mouse_motion_handler;
};

