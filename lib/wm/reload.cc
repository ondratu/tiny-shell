#include "wm/reload.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "display.h"

namespace tiny::wm {

namespace {

//! The first value of a property of a window, false when it has none
bool read_long(::Display* display, ::Window window, Atom property, Atom type,
               unsigned long& value)
{
    Atom actual = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display, window, property, 0, 1, False, type,
                           &actual, &format, &count, &after,
                           &data) != Success ||
        data == nullptr) {
        return false;
    }
    const bool found = actual == type && format == 32 && count == 1;
    if (found) {
        value = *reinterpret_cast<unsigned long*>(data);
    }
    XFree(data);
    return found;
}

std::string read_text(::Display* display, ::Window window, Atom property)
{
    XTextProperty text = {};
    std::string rv;
    if (XGetTextProperty(display, window, &text, property) != 0 &&
        text.value != nullptr && text.format == 8) {
        rv.assign(reinterpret_cast<char*>(text.value), text.nitems);
    }
    if (text.value != nullptr) {
        XFree(text.value);
    }
    return rv;
}

} // namespace

std::string request_reload(::Display* display, ::Window root)
{
    const tiny::Display& dsp = tiny::get_display();
    ::Window check = None;
    if (!read_long(display, root, dsp._NET_SUPPORTING_WM_CHECK, XA_WINDOW,
                   check)) {
        return "no window manager runs on this display";
    }
    if (read_text(display, check, dsp._NET_WM_NAME) != "tiny-shell") {
        return "the window manager that runs is not tiny-shell";
    }
    // the same message as the one that tiny-settings sends after Save; no
    // process is signalled, so a client that changed a property of the window
    // of the check can not make us hurt another process
    XEvent e = {};
    e.xclient.type = ClientMessage;
    e.xclient.window = root;
    e.xclient.message_type = dsp._TINY_SHELL_MONITORS;
    e.xclient.format = 32;
    e.xclient.data.l[0] = 2;
    if (XSendEvent(display, root, False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &e) == 0) {
        return "can't send the message to tiny-shell";
    }
    XFlush(display);
    return "";
}

} // namespace tiny::wm
