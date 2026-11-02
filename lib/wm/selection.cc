#include "wm/selection.h"

#include <poll.h>

#include <X11/Xatom.h>

#include "display.h"

namespace tiny::wm {

namespace {

//! Set by on_x_error, to know that the window of the old owner is gone
bool x_error_seen = false;

int on_x_error(::Display* /*display*/, XErrorEvent* /*error*/)
{
    x_error_seen = true;
    return 0;
}

} // namespace

std::string selection_name(int screen)
{
    return "WM_S" + std::to_string(screen);
}

Selection::Selection(::Display* display_arg, ::Window root_arg, int screen):
    display(display_arg),
    root(root_arg),
    selection(XInternAtom(display_arg, selection_name(screen).c_str(), False)),
    owner_window(
            XCreateSimpleWindow(display_arg, root_arg, -1, -1, 1, 1, 0, 0, 0))
{
    XSelectInput(display, owner_window, PropertyChangeMask);
}

Selection::~Selection()
{
    // the selection is gone with its owner
    XDestroyWindow(display, owner_window);
    XFlush(display);
}

Time Selection::server_time()
{
    XChangeProperty(display, owner_window, XA_WM_NAME, XA_STRING, 8,
                    PropModeAppend, nullptr, 0);
    XEvent e;
    XWindowEvent(display, owner_window, PropertyChangeMask, &e);
    return e.xproperty.time;
}

Selection::Result Selection::acquire(bool replace,
                                     std::chrono::milliseconds timeout)
{
    const ::Window old_owner = XGetSelectionOwner(display, selection);
    if (old_owner != None && !replace) {
        return Result::Occupied;
    }

    acquired_at = server_time();
    XSetSelectionOwner(display, selection, owner_window, acquired_at);
    if (XGetSelectionOwner(display, selection) != owner_window) {
        return Result::Failed;
    }
    if (old_owner == None) {
        return Result::Acquired;
    }
    return wait_for_destroy(old_owner, timeout) ? Result::Acquired
                                                : Result::Timeout;
}

bool Selection::wait_for_destroy(::Window owner,
                                 std::chrono::milliseconds timeout)
{
    // the owner can be gone already
    XErrorHandler old_handler = XSetErrorHandler(&on_x_error);
    x_error_seen = false;
    XSelectInput(display, owner, StructureNotifyMask);
    XSync(display, False);
    XSetErrorHandler(old_handler);
    if (x_error_seen) {
        return true;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    XEvent e;
    for (;;) {
        // other events (of the owner and ours) are not interesting now
        while (XPending(display) > 0) {
            XNextEvent(display, &e);
            if (e.type == DestroyNotify && e.xdestroywindow.window == owner) {
                return true;
            }
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            return false;
        }
        pollfd fd = {ConnectionNumber(display), POLLIN, 0};
        poll(&fd, 1, static_cast<int>(left.count()));
    }
}

void Selection::announce()
{
    XClientMessageEvent e = {};
    e.type = ClientMessage;
    e.window = root;
    e.message_type = tiny::get_display().MANAGER;
    e.format = 32;
    e.data.l[0] = static_cast<long>(acquired_at);
    e.data.l[1] = static_cast<long>(selection);
    e.data.l[2] = static_cast<long>(owner_window);
    XSendEvent(display, root, False, StructureNotifyMask,
               reinterpret_cast<XEvent*>(&e));
}

bool Selection::is_lost_by(const XSelectionClearEvent& e) const
{
    return e.selection == selection && e.window == owner_window;
}

} // namespace tiny::wm
