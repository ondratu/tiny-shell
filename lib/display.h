#pragma once

#include <X11/Xlib.h>

namespace tiny {

class Display {
  public:
    explicit Display(::Display* display);

    ~Display();

    static void init(const char* display = nullptr);

    //! The number of the open display that `get_display()` returns, 0 when
    //! there is none (it was closed, e.g. by the destructors of the globals).
    //! Something that holds a resource of the display (a font) compares it
    //! with the number it saw, and does not free the resource of a display
    //! that is gone.
    static unsigned live_serial();

    operator ::Display*() const { return display; }

    static const constexpr long _NET_WM_STATE_REMOVE = 0;
    static const constexpr long _NET_WM_STATE_ADD = 1;
    static const constexpr long _NET_WM_STATE_TOGGLE = 2;

    // Protocols
    const Atom WM_TAKE_FOCUS;
    const Atom WM_DELETE_WINDOW;

    // Properties
    const Atom WM_HINTS;
    const Atom WM_ICON_NAME;
    const Atom WM_NAME;
    const Atom WM_NORMAL_HINTS;
    const Atom WM_PROTOCOLS;
    const Atom WM_CHANGE_STATE;
    const Atom WM_STATE;

    // Types
    const Atom UTF8_STRING;
    const Atom CLIPBOARD;
    const Atom TARGETS;
    const Atom INCR;
    const Atom TINY_SELECTION; //!< the property where a selection is received

    // Manager selections (ICCCM 2.8)
    const Atom MANAGER;

    // System tray (freedesktop.org) and XEMBED
    const Atom _NET_SYSTEM_TRAY_OPCODE;
    const Atom _NET_SYSTEM_TRAY_ORIENTATION;
    const Atom _XEMBED;
    const Atom _XEMBED_INFO;

    // EWMH
    const Atom _MOTIF_WM_HINTS;
    const Atom _NET_SUPPORTED;
    const Atom _NET_SUPPORTING_WM_CHECK;
    const Atom _NET_CLIENT_LIST;
    const Atom _NET_CLIENT_LIST_STACKING;
    const Atom _NET_ACTIVE_WINDOW;
    const Atom _NET_CLOSE_WINDOW;
    const Atom _NET_FRAME_EXTENTS;
    const Atom _NET_REQUEST_FRAME_EXTENTS;
    const Atom _NET_WM_NAME;
    const Atom _NET_WM_PING;
    const Atom _NET_WM_PID;
    const Atom _NET_WM_SYNC_REQUEST;
    const Atom _NET_WM_SYNC_REQUEST_COUNTER;
    const Atom _NET_WM_MOVERESIZE;
    const Atom _NET_WM_ACTION_BELOW;
    const Atom _NET_WM_ACTION_ABOVE;
    const Atom _NET_WM_ACTION_CLOSE;
    const Atom _NET_WM_ACTION_CHANGE_DESKTOP;
    const Atom _NET_WM_ACTION_FULLSCREEN;
    const Atom _NET_WM_ACTION_MAXIMIZE_VERT;
    const Atom _NET_WM_ACTION_MAXIMIZE_HORZ;
    const Atom _NET_WM_ACTION_STICK;
    const Atom _NET_WM_ACTION_SHADE;
    const Atom _NET_WM_ACTION_MINIMIZE;
    const Atom _NET_WM_ACTION_RESIZE;
    const Atom _NET_WM_ACTION_MOVE;
    const Atom _NET_WM_ALLOWED_ACTIONS;
    const Atom _NET_WM_VISIBLE_NAME;
    const Atom _NET_WM_ICON;
    const Atom _NET_WM_ICON_NAME;
    const Atom _NET_WM_VISIBLE_ICON_NAME;
    const Atom _NET_WM_DESKTOP;
    const Atom _NET_WM_USER_TIME;
    const Atom _NET_WM_USER_TIME_WINDOW;
    const Atom _NET_WM_WINDOW_TYPE;
    const Atom _NET_WM_WINDOW_TYPE_DESKTOP;
    const Atom _NET_WM_WINDOW_TYPE_DOCK;
    const Atom _NET_WM_WINDOW_TYPE_TOOLBAR;
    const Atom _NET_WM_WINDOW_TYPE_MENU;
    const Atom _NET_WM_WINDOW_TYPE_UTILITY;
    const Atom _NET_WM_WINDOW_TYPE_SPLASH;
    const Atom _NET_WM_WINDOW_TYPE_DIALOG;
    const Atom _NET_WM_WINDOW_TYPE_NOTIFICATION;
    const Atom _NET_WM_WINDOW_TYPE_POPUP_MENU;
    const Atom _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
    const Atom _NET_WM_WINDOW_TYPE_TOOLTIP;
    const Atom _NET_WM_WINDOW_TYPE_COMBO;
    const Atom _NET_WM_WINDOW_TYPE_DND;
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
    const Atom _NET_WM_STRUT;
    const Atom _NET_WM_STRUT_PARTIAL;
    const Atom _NET_WORKAREA;
    //! A ClientMessage to the root window, to the shell: data[0] is 0 (hide the
    //! numbers of the monitors), 1 (show them while the window data[1] lives,
    //! tiny-settings does it) or 2 (read `[monitors]` of the files again)
    const Atom _TINY_SHELL_MONITORS;
    const Atom _NET_DESKTOP_VIEWPORT;
    const Atom _NET_DESKTOP_GEOMETRY;
    const Atom _NET_CURRENT_DESKTOP;
    const Atom _NET_NUMBER_OF_DESKTOPS;

  private:
    ::Display* display;
    unsigned serial; //!< see `live_serial()`
};

Display& get_display();

} // namespace tiny
