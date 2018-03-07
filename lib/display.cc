#include <memory>

#include "display.h"
#include "object.h"

namespace tiny {

std::shared_ptr<Display> display(nullptr);

Display::Display(::Display *display):
    display(display),
    WM_TAKE_FOCUS(XInternAtom(display, "WM_TAKE_FOCUS", 0)),
    WM_DELETE_WINDOW(XInternAtom(display, "WM_DELETE_WINDOW", 0)),
    WM_NAME(XInternAtom(display, "WM_NAME", 0)),
    WM_PROTOCOLS(XInternAtom(display, "WM_PROTOCOLS", 0))
{}

Display::~Display(){
    XCloseDisplay(display);
}

void Display::init(const char *display_name)
{
    ::Display * display = XOpenDisplay(display_name);
    if (display == nullptr) {
        throw std::runtime_error("Failed to open X display");
    }

    tiny::display = std::shared_ptr<Display>(new Display(display));
}



Display& get_display(){
    if (display.get() == nullptr){
        error("Display is not initialized!");
        abort();
    }
    return *display.get();
}

} // namespace tiny
