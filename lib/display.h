#pragma once

#include <X11/Xlib.h>

namespace tiny {

class Display {
  public:
    Display(::Display *display);

    ~Display();

    static void init(const char *display=nullptr);

    inline operator ::Display* () const
    { return display; }

    // Protocols
    const Atom WM_TAKE_FOCUS;
    const Atom WM_DELETE_WINDOW;

    // Properties
    const Atom WM_NAME;
    const Atom WM_PROTOCOLS;

    // EWMH
    const Atom UTF8_STRING;
    const Atom _NET_SUPPORTED;
    const Atom _NET_WM_NAME;
    const Atom _NET_WM_VISIBLE_NAME;
    const Atom _NET_WM_ICON_NAME;
    const Atom _NET_WM_VISIBLE_ICON_NAME;
    const Atom _NET_WM_DESKTOP;
    const Atom _NET_WM_WINDOW_TYPE;
    const Atom _NET_WM_WINDOW_TYPE_DESKTOP;
    const Atom _NET_WM_WINDOW_TYPE_DOCK;
    const Atom _NET_WM_WINDOW_TYPE_TOOLBAR;
    const Atom _NET_WM_WINDOW_TYPE_MENU;
    const Atom _NET_WM_WINDOW_TYPE_UTILITY;
    const Atom _NET_WM_WINDOW_TYPE_SPLASH;
    const Atom _NET_WM_WINDOW_TYPE_DIALOG;
    const Atom _NET_WM_WINDOW_TYPE_NORMAL;
    const Atom _NET_WM_STATE;
    const Atom _NET_WM_STATE_MODAL;
    const Atom _NET_WM_STATE_STICKY;
    const Atom _NET_WM_STATE_MAXIMIZED_VERT;
    const Atom _NET_WM_STATE_MAXIMIZED_HORZ;
    const Atom _NET_WM_STATE_SHADED;
    const Atom _NET_WM_STATE_SKIP_TASKBAR;
    const Atom _NET_WM_STATE_SKIP_PAGER;
    const Atom _NET_WM_STATE_HIDDEN;
    const Atom _NET_WM_STATE_FULLSCREEN;
    const Atom _NET_WM_STATE_ABOVE;
    const Atom _NET_WM_STATE_BELOW;
    const Atom _NET_WM_STATE_DEMANDS_ATTENTION;

    const char *EWMH[29] = {
        "UTF8_STRING",
        "_NET_SUPPORTED",
        "_NET_WM_NAME",
        "_NET_WM_VISIBLE_NAME",
        "_NET_WM_ICON_NAME",
        "_NET_WM_VISIBLE_ICON_NAME",
        "_NET_WM_DESKTOP",
        "_NET_WM_WINDOW_TYPE",
        "_NET_WM_WINDOW_TYPE_DESKTOP",
        "_NET_WM_WINDOW_TYPE_DOCK",
        "_NET_WM_WINDOW_TYPE_TOOLBAR",
        "_NET_WM_WINDOW_TYPE_MENU",
        "_NET_WM_WINDOW_TYPE_UTILITY",
        "_NET_WM_WINDOW_TYPE_SPLASH",
        "_NET_WM_WINDOW_TYPE_DIALOG",
        "_NET_WM_WINDOW_TYPE_NORMAL",
        "_NET_WM_STATE",
        "_NET_WM_STATE_MODAL",
        "_NET_WM_STATE_STICKY",
        "_NET_WM_STATE_MAXIMIZED_VERT",
        "_NET_WM_STATE_MAXIMIZED_HORZ",
        "_NET_WM_STATE_SHADED",
        "_NET_WM_STATE_SKIP_TASKBAR",
        "_NET_WM_STATE_SKIP_PAGER",
        "_NET_WM_STATE_HIDDEN",
        "_NET_WM_STATE_FULLSCREEN",
        "_NET_WM_STATE_ABOVE",
        "_NET_WM_STATE_BELOW",
        "_NET_WM_STATE_DEMANDS_ATTENTION"
    };

  private:
    ::Display * display;
};


Display& get_display();

}
