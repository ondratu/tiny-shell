#pragma once

#include <X11/Xutil.h>

#include <memory>
#include <list>

#include "wm_widget.h"
#include "wm_title.h"
#include "wm_corner.h"
#include "wm_buttons.h"

class WMWindow: public WMWidget {
  public:
    WMWindow(Display * display, Window parent, Window child,
             int x, int y, uint32_t width, uint32_t height);

    virtual ~WMWindow();

    static WMWindow * create(Display * display, Window parent, Window child);

    virtual void map_all();

    virtual void set_events();

    inline const bool get_minimized() const
    { return is_minimized; }

    void set_focus();

    void close();

    // XXX: could be named iconify....
    void minimize();

    void restore();

    /* signal handlers */
    void on_close_click(WMObject *o, const XEvent &e, void *data);

    void on_minimize_click(WMObject *o, const XEvent &e, void *data);

    void on_title_drag_begin(WMObject *o, const XEvent &e, void *data);

    void on_title_drag_motion(WMObject *o, const XEvent &e, void *data);

    void on_corner_drag_begin(WMObject *o, const XEvent &e, void *data);

    void on_corner_drag_motion(WMObject *o, const XEvent &e, void *data);

    /* event handlers */
    void on_button_press(const XEvent &e, void *data);

    void on_focus_in(const XEvent &e, void *data);

    void on_focus_out(const XEvent &e, void *data);

  protected:
    Window child;
    std::list<std::shared_ptr<WMWidget>> children;

    std::shared_ptr<WMTitle> title;

    std::shared_ptr<WMCloseButton> close_btn;
    std::shared_ptr<WMButton> maxim_btn;
    std::shared_ptr<WMButton> minim_btn;

    std::shared_ptr<WMCorner> left_top;
    std::shared_ptr<WMCorner> left_bottom;
    std::shared_ptr<WMCorner> right_top;
    std::shared_ptr<WMCorner> right_bottom;

  private:
    bool resizable = true;

    bool is_minimized = false;
    bool resizing = false;

    XEvent start_event;                 // state before moving/resizing
    XWindowAttributes start_attrs;

    XSizeHints * hints;
};
