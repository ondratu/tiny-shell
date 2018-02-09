#include <X11/Xutil.h>

#include "wm_buttons.h"
#include "wm_title.h"
#include "wm_corner.h"

#include <memory>

class WMWindow : public WMWidget {
  private:
    WMWindow(Display * display, Window parent, Window child,
             int x, int y, int width, int height);

  public:
    static WMWindow* create(Display * display, Window parent, Window child);

    ~WMWindow();

    void set_focus();

    static void close_window(WMWidget &w, const XEvent &e, void * data);

    static void minimize_window(WMWidget &w, const XEvent &e, void * data);

    void do_close_window();

    static void title_press(WMTitle &title, const XEvent &e, void * data);

    static void title_release(WMTitle &title, const XEvent &e, void * data);

    static void title_motion(WMTitle &title, const XEvent &e, void * data);

    static void corner_press(WMWidget &w, const XEvent &e, void * data);

    static void corner_release(WMWidget &w, const XEvent &e, void * data);

    static void corner_motion(WMWidget &w, const XEvent &e, void * data);

    std::shared_ptr<WMCloseButton> close_btn;
    std::shared_ptr<WMButton> maxim_btn;
    std::shared_ptr<WMButton> minim_btn;

    bool is_minimized = false;

  protected:
    static void on_click(const XEvent &e, void* w, void* data);
    static void on_focus_in(const XEvent &e, void* w, void* data);
    static void on_focus_out(const XEvent &e, void* w, void* data);

    Window child;
    XSizeHints * hints;
    std::shared_ptr<WMTitle> title;
    bool moving;
    bool resizing;
    XEvent start_event;
    XWindowAttributes start_attrs;

    std::shared_ptr<WMCorner> left_top;
    std::shared_ptr<WMCorner> left_bottom;
    std::shared_ptr<WMCorner> right_top;
    std::shared_ptr<WMCorner> right_bottom;
};
