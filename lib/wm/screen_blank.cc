#include "wm/screen_blank.h"

namespace tiny::wm {

ScreenBlank::~ScreenBlank()
{
    if (window != None) {
        XDestroyWindow(display, window);
    }
    if (cursor != None) {
        XFreeCursor(display, cursor);
    }
}

void ScreenBlank::show()
{
    const int screen = DefaultScreen(display);
    if (window == None) {
        XSetWindowAttributes attributes;
        attributes.override_redirect = True;
        attributes.background_pixel = BlackPixel(display, screen);
        window = XCreateWindow(
                display, DefaultRootWindow(display), 0, 0,
                static_cast<unsigned>(DisplayWidth(display, screen)),
                static_cast<unsigned>(DisplayHeight(display, screen)), 0,
                CopyFromParent, InputOutput, CopyFromParent,
                CWOverrideRedirect | CWBackPixel, &attributes);
        const char empty = 0;
        const Pixmap bitmap =
                XCreateBitmapFromData(display, window, &empty, 1, 1);
        XColor black = {};
        cursor = XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0,
                                     0);
        XFreePixmap(display, bitmap);
        XDefineCursor(display, window, cursor);
    }
    // the screen can have changed its size (RandR) since the last time
    XMoveResizeWindow(display, window, 0, 0,
                      static_cast<unsigned>(DisplayWidth(display, screen)),
                      static_cast<unsigned>(DisplayHeight(display, screen)));
    XMapRaised(display, window);
    XFlush(display);
    visible = true;
}

void ScreenBlank::hide()
{
    if (window != None) {
        XUnmapWindow(display, window);
        XFlush(display);
    }
    visible = false;
}

} // namespace tiny::wm
