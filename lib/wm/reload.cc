#include "wm/reload.h"

#include <climits>
#include <csignal>

#include <sys/types.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "display.h"
#include "wm/process.h"

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
    unsigned long check = None;
    if (!read_long(display, root, dsp._NET_SUPPORTING_WM_CHECK, XA_WINDOW,
                   check)) {
        return "no window manager runs on this display";
    }
    if (read_text(display, static_cast<::Window>(check), dsp._NET_WM_NAME) !=
        "tiny-shell") {
        return "the window manager that runs is not tiny-shell";
    }
    unsigned long pid = 0;
    if (!read_long(display, static_cast<::Window>(check), dsp._NET_WM_PID,
                   XA_CARDINAL, pid) ||
        pid <= 1 || pid > static_cast<unsigned long>(INT_MAX)) {
        return "tiny-shell does not tell its process";
    }
    if (read_text(display, static_cast<::Window>(check),
                  XA_WM_CLIENT_MACHINE) != local_host_name()) {
        return "tiny-shell runs on another computer";
    }
    if (kill(static_cast<pid_t>(pid), SIGHUP) != 0) {
        return "can't signal the process of tiny-shell";
    }
    return "";
}

} // namespace tiny::wm
