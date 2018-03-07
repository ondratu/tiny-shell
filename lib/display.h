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

  private:
    ::Display * display;
};


Display& get_display();

}
