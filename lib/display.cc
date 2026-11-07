#include <memory>
#include <stdexcept>

#include "display.h"
#include "object.h"

namespace tiny {

std::shared_ptr<Display> display(nullptr);

namespace {

unsigned last_serial = 0;
unsigned open_serial = 0; //!< of the display that is open now

} // namespace

Display::Display(::Display* display_arg):
    WM_TAKE_FOCUS(XInternAtom(display_arg, "WM_TAKE_FOCUS", 0)),
    WM_DELETE_WINDOW(XInternAtom(display_arg, "WM_DELETE_WINDOW", 0)),
    WM_HINTS(XInternAtom(display_arg, "WM_HINTS", 0)),
    WM_ICON_NAME(XInternAtom(display_arg, "WM_ICON_NAME", 0)),
    WM_NAME(XInternAtom(display_arg, "WM_NAME", 0)),
    WM_NORMAL_HINTS(XInternAtom(display_arg, "WM_NORMAL_HINTS", 0)),
    WM_PROTOCOLS(XInternAtom(display_arg, "WM_PROTOCOLS", 0)),
    WM_CHANGE_STATE(XInternAtom(display_arg, "WM_CHANGE_STATE", 0)),
    WM_STATE(XInternAtom(display_arg, "WM_STATE", 0)),
    UTF8_STRING(XInternAtom(display_arg, "UTF8_STRING", 0)),
    CLIPBOARD(XInternAtom(display_arg, "CLIPBOARD", 0)),
    TARGETS(XInternAtom(display_arg, "TARGETS", 0)),
    INCR(XInternAtom(display_arg, "INCR", 0)),
    TINY_SELECTION(XInternAtom(display_arg, "TINY_SELECTION", 0)),
    MANAGER(XInternAtom(display_arg, "MANAGER", 0)),
    _NET_SYSTEM_TRAY_OPCODE(
            XInternAtom(display_arg, "_NET_SYSTEM_TRAY_OPCODE", 0)),
    _NET_SYSTEM_TRAY_ORIENTATION(
            XInternAtom(display_arg, "_NET_SYSTEM_TRAY_ORIENTATION", 0)),
    _XEMBED(XInternAtom(display_arg, "_XEMBED", 0)),
    _XEMBED_INFO(XInternAtom(display_arg, "_XEMBED_INFO", 0)),
    _MOTIF_WM_HINTS(XInternAtom(display_arg, "_MOTIF_WM_HINTS", 0)),
    _NET_SUPPORTED(XInternAtom(display_arg, "_NET_SUPPORTED", 0)),
    _NET_SUPPORTING_WM_CHECK(
            XInternAtom(display_arg, "_NET_SUPPORTING_WM_CHECK", 0)),
    _NET_CLIENT_LIST(XInternAtom(display_arg, "_NET_CLIENT_LIST", 0)),
    _NET_CLIENT_LIST_STACKING(
            XInternAtom(display_arg, "_NET_CLIENT_LIST_STACKING", 0)),
    _NET_ACTIVE_WINDOW(XInternAtom(display_arg, "_NET_ACTIVE_WINDOW", 0)),
    _NET_CLOSE_WINDOW(XInternAtom(display_arg, "_NET_CLOSE_WINDOW", 0)),
    _NET_FRAME_EXTENTS(XInternAtom(display_arg, "_NET_FRAME_EXTENTS", 0)),
    _NET_REQUEST_FRAME_EXTENTS(
            XInternAtom(display_arg, "_NET_REQUEST_FRAME_EXTENTS", 0)),
    _NET_WM_NAME(XInternAtom(display_arg, "_NET_WM_NAME", 0)),
    _NET_WM_PING(XInternAtom(display_arg, "_NET_WM_PING", 0)),
    _NET_WM_PID(XInternAtom(display_arg, "_NET_WM_PID", 0)),
    _NET_WM_SYNC_REQUEST(XInternAtom(display_arg, "_NET_WM_SYNC_REQUEST", 0)),
    _NET_WM_SYNC_REQUEST_COUNTER(
            XInternAtom(display_arg, "_NET_WM_SYNC_REQUEST_COUNTER", 0)),
    _NET_WM_MOVERESIZE(XInternAtom(display_arg, "_NET_WM_MOVERESIZE", 0)),
    _NET_WM_ACTION_BELOW(XInternAtom(display_arg, "_NET_WM_ACTION_BELOW", 0)),
    _NET_WM_ACTION_ABOVE(XInternAtom(display_arg, "_NET_WM_ACTION_ABOVE", 0)),
    _NET_WM_ACTION_CLOSE(XInternAtom(display_arg, "_NET_WM_ACTION_CLOSE", 0)),
    _NET_WM_ACTION_CHANGE_DESKTOP(
            XInternAtom(display_arg, "_NET_WM_ACTION_CHANGE_DESKTOP", 0)),
    _NET_WM_ACTION_FULLSCREEN(
            XInternAtom(display_arg, "_NET_WM_ACTION_FULLSCREEN", 0)),
    _NET_WM_ACTION_MAXIMIZE_VERT(
            XInternAtom(display_arg, "_NET_WM_ACTION_MAXIMIZE_VERT", 0)),
    _NET_WM_ACTION_MAXIMIZE_HORZ(
            XInternAtom(display_arg, "_NET_WM_ACTION_MAXIMIZE_HORZ", 0)),
    _NET_WM_ACTION_STICK(XInternAtom(display_arg, "_NET_WM_ACTION_STICK", 0)),
    _NET_WM_ACTION_SHADE(XInternAtom(display_arg, "_NET_WM_ACTION_SHADE", 0)),
    _NET_WM_ACTION_MINIMIZE(
            XInternAtom(display_arg, "_NET_WM_ACTION_MINIMIZE", 0)),
    _NET_WM_ACTION_RESIZE(XInternAtom(display_arg, "_NET_WM_ACTION_RESIZE", 0)),
    _NET_WM_ACTION_MOVE(XInternAtom(display_arg, "_NET_WM_ACTION_MOVE", 0)),
    _NET_WM_ALLOWED_ACTIONS(
            XInternAtom(display_arg, "_NET_WM_ALLOWED_ACTIONS", 0)),
    _NET_WM_VISIBLE_NAME(XInternAtom(display_arg, "_NET_WM_VISIBLE_NAME", 0)),
    _NET_WM_ICON(XInternAtom(display_arg, "_NET_WM_ICON", 0)),
    _NET_WM_ICON_NAME(XInternAtom(display_arg, "_NET_WM_ICON_NAME", 0)),
    _NET_WM_VISIBLE_ICON_NAME(
            XInternAtom(display_arg, "_NET_WM_VISIBLE_ICON_NAME", 0)),
    _NET_WM_DESKTOP(XInternAtom(display_arg, "_NET_WM_DESKTOP", 0)),
    _NET_WM_USER_TIME(XInternAtom(display_arg, "_NET_WM_USER_TIME", 0)),
    _NET_WM_USER_TIME_WINDOW(
            XInternAtom(display_arg, "_NET_WM_USER_TIME_WINDOW", 0)),
    _NET_WM_WINDOW_TYPE(XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE", 0)),
    _NET_WM_WINDOW_TYPE_DESKTOP(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_DESKTOP", 0)),
    _NET_WM_WINDOW_TYPE_DOCK(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_DOCK", 0)),
    _NET_WM_WINDOW_TYPE_TOOLBAR(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_TOOLBAR", 0)),
    _NET_WM_WINDOW_TYPE_MENU(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_MENU", 0)),
    _NET_WM_WINDOW_TYPE_UTILITY(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_UTILITY", 0)),
    _NET_WM_WINDOW_TYPE_SPLASH(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_SPLASH", 0)),
    _NET_WM_WINDOW_TYPE_DIALOG(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_DIALOG", 0)),
    _NET_WM_WINDOW_TYPE_NOTIFICATION(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_NOTIFICATION", 0)),
    _NET_WM_WINDOW_TYPE_POPUP_MENU(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_POPUP_MENU", 0)),
    _NET_WM_WINDOW_TYPE_DROPDOWN_MENU(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", 0)),
    _NET_WM_WINDOW_TYPE_TOOLTIP(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_TOOLTIP", 0)),
    _NET_WM_WINDOW_TYPE_COMBO(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_COMBO", 0)),
    _NET_WM_WINDOW_TYPE_DND(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_DND", 0)),
    _NET_WM_WINDOW_TYPE_NORMAL(
            XInternAtom(display_arg, "_NET_WM_WINDOW_TYPE_NORMAL", 0)),
    _NET_WM_STATE(XInternAtom(display_arg, "_NET_WM_STATE", 0)),
    _NET_WM_STATE_MODAL(XInternAtom(display_arg, "_NET_WM_STATE_MODAL", 0)),
    _NET_WM_STATE_STICKY(XInternAtom(display_arg, "_NET_WM_STATE_STICKY", 0)),
    _NET_WM_STATE_MAXIMIZED_VERT(
            XInternAtom(display_arg, "_NET_WM_STATE_MAXIMIZED_VERT", 0)),
    _NET_WM_STATE_MAXIMIZED_HORZ(
            XInternAtom(display_arg, "_NET_WM_STATE_MAXIMIZED_HORZ", 0)),
    _NET_WM_STATE_SHADED(XInternAtom(display_arg, "_NET_WM_STATE_SHADED", 0)),
    _NET_WM_STATE_SKIP_TASKBAR(
            XInternAtom(display_arg, "_NET_WM_STATE_SKIP_TASKBAR", 0)),
    _NET_WM_STATE_SKIP_PAGER(
            XInternAtom(display_arg, "_NET_WM_STATE_SKIP_PAGER", 0)),
    _NET_WM_STATE_HIDDEN(XInternAtom(display_arg, "_NET_WM_STATE_HIDDEN", 0)),
    _NET_WM_STATE_FULLSCREEN(
            XInternAtom(display_arg, "_NET_WM_STATE_FULLSCREEN", 0)),
    _NET_WM_STATE_ABOVE(XInternAtom(display_arg, "_NET_WM_STATE_ABOVE", 0)),
    _NET_WM_STATE_BELOW(XInternAtom(display_arg, "_NET_WM_STATE_BELOW", 0)),
    _NET_WM_STATE_DEMANDS_ATTENTION(
            XInternAtom(display_arg, "_NET_WM_STATE_DEMANDS_ATTENTION", 0)),
    _NET_WM_STATE_FOCUSED(XInternAtom(display_arg, "_NET_WM_STATE_FOCUSED", 0)),
    _NET_WM_STRUT(XInternAtom(display_arg, "_NET_WM_STRUT", 0)),
    _NET_WM_STRUT_PARTIAL(XInternAtom(display_arg, "_NET_WM_STRUT_PARTIAL", 0)),
    _NET_WORKAREA(XInternAtom(display_arg, "_NET_WORKAREA", 0)),
    _TINY_SHELL_MONITORS(XInternAtom(display_arg, "_TINY_SHELL_MONITORS", 0)),
    _NET_DESKTOP_VIEWPORT(XInternAtom(display_arg, "_NET_DESKTOP_VIEWPORT", 0)),
    _NET_DESKTOP_GEOMETRY(XInternAtom(display_arg, "_NET_DESKTOP_GEOMETRY", 0)),
    _NET_CURRENT_DESKTOP(XInternAtom(display_arg, "_NET_CURRENT_DESKTOP", 0)),
    _NET_NUMBER_OF_DESKTOPS(
            XInternAtom(display_arg, "_NET_NUMBER_OF_DESKTOPS", 0)),
    _NET_SHOWING_DESKTOP(XInternAtom(display_arg, "_NET_SHOWING_DESKTOP", 0)),
    _NET_DESKTOP_NAMES(XInternAtom(display_arg, "_NET_DESKTOP_NAMES", 0)),
    display(display_arg),
    serial(++last_serial)
{
    open_serial = serial;
}

Display::~Display()
{
    if (open_serial == serial) { // a newer display may be open already
        open_serial = 0;
    }
    XCloseDisplay(display);
}

unsigned Display::live_serial()
{
    return open_serial;
}

void Display::init(const char* display_name)
{
    ::Display* dsp = XOpenDisplay(display_name);
    if (dsp == nullptr) {
        throw std::runtime_error("Failed to open X display");
    }

    // NOLINTNEXTLINE(modernize-make-shared): the constructor is private
    tiny::display = std::shared_ptr<Display>(new Display(dsp));
}

Display& get_display()
{
    if (display == nullptr) {
        error("Display is not initialized!");
        abort();
    }
    return *display.get();
}

} // namespace tiny
