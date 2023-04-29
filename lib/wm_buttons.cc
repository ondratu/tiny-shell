#include <X11/cursorfont.h>

#include "wm_buttons.h"
#include "x_util.h"

namespace wm {

Button::Button():
    tiny::Button(
        tiny::theme.wm_win_header-2*get_padding(),      // TODO: WM_WIN_HEADER_PADDING
        tiny::theme.wm_win_header-2*get_padding())
{
}

Button::~Button(){
    disconnect(ExposureMask);
}

void Button::set_events(long mask)
{
    tiny::Button::set_events(ExposureMask|mask);

    connect(Expose,
            static_cast<tiny::event_signal_t>(&Button::on_expose));
}

void Button::on_enter_notify(const XEvent &e, void * data){
    tiny::Button::on_enter_notify(e, data);
    XSetWindowBorder(display, window, tiny::theme.wm_button.get_br(get_theme_state()));
}

void Button::on_leave_notify(const XEvent &e, void * data){
    tiny::Button::on_leave_notify(e, data);
    XSetWindowBorder(display, window, tiny::theme.wm_button.get_br(get_theme_state()));
}



CloseButton::CloseButton():Button()
{}

CloseButton::~CloseButton()
{}

void CloseButton::on_expose(const XEvent &e, void *data)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, tiny::theme.wm_button.get_fg(get_theme_state()));
    XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinBevel);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    XDrawLine(display, window, gc,
        mid_x-3.5, mid_y-3.5, mid_x+3.5, mid_y+3.5);
    XDrawLine(display, window, gc,
        mid_x+3.5, mid_y-3.5, mid_x-3.5, mid_y+3.5);
    XFreeGC(display, gc);
}



MaximizeButton::MaximizeButton():Button(), restore(false)
{}

MaximizeButton::~MaximizeButton()
{}

void MaximizeButton::on_expose(const XEvent &e, void *data)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, tiny::theme.wm_button.get_fg(get_theme_state()));
    XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinMiter);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    if (restore){
        XPoint points[5] = {
            tiny::x_point(mid_x-2, mid_y-1),
            tiny::x_point(mid_x+2, mid_y-1),
            tiny::x_point(mid_x+2, mid_y+3),
            tiny::x_point(mid_x-2, mid_y+3),
            tiny::x_point(mid_x-2, mid_y-1)};
        // maybe CoordModePrevious
        XDrawLines(display, window, gc, points, 5, CoordModeOrigin);
    } else {
        XPoint points[5] = {
            tiny::x_point(mid_x-3, mid_y-3),
            tiny::x_point(mid_x+3, mid_y-3),
            tiny::x_point(mid_x+3, mid_y+3),
            tiny::x_point(mid_x-3, mid_y+3),
            tiny::x_point(mid_x-3, mid_y-3)};
        // maybe CoordModePrevious
        XDrawLines(display, window, gc, points, 5, CoordModeOrigin);
    }
    XFreeGC(display, gc);
}

void MaximizeButton::set_restore(bool _restore)
{
    restore = _restore;
    XClearWindow(display, window);
    on_expose(XEvent(), nullptr);
}


MinimizeButton::MinimizeButton():Button()
{}

MinimizeButton::~MinimizeButton()
{}

void MinimizeButton::on_expose(const XEvent &e, void *data)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, tiny::theme.wm_button.get_fg(get_theme_state()));
    XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinBevel);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    XDrawLine(display, window, gc,
        mid_x-4, mid_y+3.5, mid_x+4, mid_y+3.5);
    XFreeGC(display, gc);
}

} // namespace wm
