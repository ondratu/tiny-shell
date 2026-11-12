#include "wm/session_start.h"

#include <X11/Xatom.h>

namespace tiny::wm {

bool claim_autostart(::Display* display, ::Window root)
{
    const Atom marker = XInternAtom(display, "_TINY_SHELL_AUTOSTART", False);

    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    const bool known = XGetWindowProperty(display, root, marker, 0, 1, False,
                                          XA_CARDINAL, &type, &format, &count,
                                          &after, &data) == Success &&
                       type == XA_CARDINAL;
    if (data != nullptr) {
        XFree(data);
    }
    if (known) {
        return false;
    }
    const unsigned long started = 1;
    XChangeProperty(display, root, marker, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&started), 1);
    return true;
}

} // namespace tiny::wm
