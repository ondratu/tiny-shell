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

    static const constexpr long _NET_WM_STATE_REMOVE = 0;
    static const constexpr long _NET_WM_STATE_ADD = 1;
    static const constexpr long _NET_WM_STATE_TOGGLE = 2;

    // Protocols
    const Atom WM_TAKE_FOCUS;
    const Atom WM_DELETE_WINDOW;

    // Properties
    const Atom WM_NAME;
    const Atom WM_NORMAL_HINTS;
    const Atom WM_PROTOCOLS;

    // EWMH
    const Atom UTF8_STRING;
    const Atom _MOTIF_WM_HINTS;
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
    const Atom _NET_WM_STATE_FOCUSED;

  private:
    ::Display * display;
};


Display& get_display();

}
