#pragma once

#include "wm_widget.h"
#include "wm_buttons.h"

class WMButton : public WMWidget {
  public:
    WMButton(Display * display, Window parent,
             int x, int y, int width, int height);

    virtual ~WMButton();

    /* set static handler to handlers */
    void connect(const std::string &signal, on_event_t handler, void * data);

  protected:
    /* when static handler call, call own hanlder
     * w - pointer to widget on which window event is
     * data pointer to user data
    */
    static void on_click(const XEvent &e, void* w, void* data);
    static void on_expose(const XEvent &e, void* w, void *);
    static void on_enter_notify(const XEvent &e, void* w, void *);
    static void on_leave_notify(const XEvent &e, void* w, void *);

    virtual void expose(const XEvent &e, void *);

  private:
    on_event_t on_click_handler = nullptr;
};

class WMCloseButton : public WMButton {
  public:
    WMCloseButton(Display * display, Window parent,
                  int x, int y, int width, int height);

    ~WMCloseButton();

    virtual void expose(const XEvent &e, void *);
};

class WMMinimizeButton : public WMButton {
  public:
    WMMinimizeButton(Display * display, Window parent,
                  int x, int y, int width, int height);

    ~WMMinimizeButton();

    virtual void expose(const XEvent &e, void *);
};
