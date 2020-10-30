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

    const char *EWMH[3] = {
        "UTF8_STRING",
        "_NET_SUPPORTED",
        "_NET_WM_NAME",
    };

  private:
    ::Display * display;
};


Display& get_display();

}
