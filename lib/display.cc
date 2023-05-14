#include <memory>
#include <stdexcept>

#include "display.h"
#include "object.h"

namespace tiny {

std::shared_ptr<Display> display(nullptr);

Display::Display(::Display *display):
    WM_TAKE_FOCUS(XInternAtom(display, "WM_TAKE_FOCUS", 0)),
    WM_DELETE_WINDOW(XInternAtom(display, "WM_DELETE_WINDOW", 0)),
    WM_HINTS(XInternAtom(display, "WM_HINTS", 0)),
    WM_ICON_NAME(XInternAtom(display, "WM_ICON_NAME", 0)),
    WM_NAME(XInternAtom(display, "WM_NAME", 0)),
    WM_NORMAL_HINTS(XInternAtom(display, "WM_NORMAL_HINTS", 0)),
    WM_PROTOCOLS(XInternAtom(display, "WM_PROTOCOLS", 0)),
    UTF8_STRING(XInternAtom(display, "UTF8_STRING", 0)),
    _MOTIF_WM_HINTS(XInternAtom(display, "_MOTIF_WM_HINTS", 0)),
    _NET_SUPPORTED(XInternAtom(display, "_NET_SUPPORTED", 0)),
    _NET_WM_NAME(XInternAtom(display, "_NET_WM_NAME", 0)),
    _NET_WM_VISIBLE_NAME(XInternAtom(display, "_NET_WM_VISIBLE_NAME", 0)),
    _NET_WM_ICON(XInternAtom(display, "_NET_WM_ICON", 0)),
    _NET_WM_ICON_NAME(XInternAtom(display, "_NET_WM_ICON_NAME", 0)),
    _NET_WM_VISIBLE_ICON_NAME(XInternAtom(display, "_NET_WM_VISIBLE_ICON_NAME", 0)),
    _NET_WM_DESKTOP(XInternAtom(display, "_NET_WM_DESKTOP", 0)),
    _NET_WM_WINDOW_TYPE(XInternAtom(display, "_NET_WM_WINDOW_TYPE", 0)),
    _NET_WM_WINDOW_TYPE_DESKTOP(XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", 0)),
    _NET_WM_WINDOW_TYPE_DOCK(XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", 0)),
    _NET_WM_WINDOW_TYPE_TOOLBAR(XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", 0)),
    _NET_WM_WINDOW_TYPE_MENU(XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", 0)),
    _NET_WM_WINDOW_TYPE_UTILITY(XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", 0)),
    _NET_WM_WINDOW_TYPE_SPLASH(XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", 0)),
    _NET_WM_WINDOW_TYPE_DIALOG(XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", 0)),
    _NET_WM_WINDOW_TYPE_NORMAL(XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", 0)),
    _NET_WM_STATE(XInternAtom(display, "_NET_WM_STATE", 0)),
    _NET_WM_STATE_MODAL(XInternAtom(display, "_NET_WM_STATE_MODAL", 0)),
    _NET_WM_STATE_STICKY(XInternAtom(display, "_NET_WM_STATE_STICKY", 0)),
    _NET_WM_STATE_MAXIMIZED_VERT(XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", 0)),
    _NET_WM_STATE_MAXIMIZED_HORZ(XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", 0)),
    _NET_WM_STATE_SHADED(XInternAtom(display, "_NET_WM_STATE_SHADED", 0)),
    _NET_WM_STATE_SKIP_TASKBAR(XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", 0)),
    _NET_WM_STATE_SKIP_PAGER(XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", 0)),
    _NET_WM_STATE_HIDDEN(XInternAtom(display, "_NET_WM_STATE_HIDDEN", 0)),
    _NET_WM_STATE_FULLSCREEN(XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0)),
    _NET_WM_STATE_ABOVE(XInternAtom(display, "_NET_WM_STATE_ABOVE", 0)),
    _NET_WM_STATE_BELOW(XInternAtom(display, "_NET_WM_STATE_BELOW", 0)),
    _NET_WM_STATE_DEMANDS_ATTENTION(XInternAtom(display, "_NET_WM_STATE_DEMANDS_ATTENTION", 0)),
    _NET_WM_STATE_FOCUSED(XInternAtom(display, "_NET_WM_STATE_FOCUSED", 0)),
    display(display)
{}

Display::~Display(){
    XCloseDisplay(display);
}

void Display::init(const char *display_name)
{
    ::Display* display = XOpenDisplay(display_name);
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
