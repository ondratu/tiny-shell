#include <math.h>
#include <algorithm>

#include <X11/cursorfont.h>

#include "wm_buttons.h"
#include "x_util.h"

namespace wm {

// Symbol geometry is designed for scale 1 and follows TINY_SCALE_FACTOR
static double sc(double value)
{ return value * tiny::theme.scale_factor; }

// Truncates like the implicit conversion to a pixel coordinate did
static int px(double value)
{ return static_cast<int>(value); }

static int line_width()
{ return std::max(1, int(lround(sc(2)))); }

Button::Button():
    tiny::Button(
        tiny::theme.wm_win_icon,      // TODO: WM_WIN_HEADER_PADDING
        tiny::theme.wm_win_icon)
{
}

Button::~Button(){
    if (event_done) {
        disconnect(Expose);
    }
}

void Button::set_events(long mask)
{
    tiny::Button::set_events(ExposureMask|mask);

    connect(Expose,
            static_cast<tiny::event_signal_t>(&Button::on_expose));
}

void Button::on_enter_notify(const XEvent &e, void *data)
{
    tiny::Button::on_enter_notify(e, data);
    on_expose(e, nullptr);
}

void Button::on_leave_notify(const XEvent &e, void *data)
{
    tiny::Button::on_leave_notify(e, data);
    on_expose(e, nullptr);
}


CloseButton::CloseButton():Button()
{}

CloseButton::~CloseButton()
{}

void CloseButton::on_expose(const XEvent &, void *)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    uint8_t state = get_theme_state();

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);

    XSetForeground(display, gc, get_style().get_fg(state));
    XSetLineAttributes(display, gc, line_width(), LineSolid, CapButt, JoinBevel);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    XDrawLine(display, window, gc,
        px(mid_x-sc(3.5)), px(mid_y-sc(3.5)), px(mid_x+sc(3.5)), px(mid_y+sc(3.5)));
    XDrawLine(display, window, gc,
        px(mid_x+sc(3.5)), px(mid_y-sc(3.5)), px(mid_x-sc(3.5)), px(mid_y+sc(3.5)));
    XFreeGC(display, gc);
}



MaximizeButton::MaximizeButton():Button(), restore(false)
{}

MaximizeButton::~MaximizeButton()
{}

void MaximizeButton::on_expose(const XEvent &, void *)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);
    uint8_t state = get_theme_state();

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);

    XSetForeground(display, gc, get_style().get_fg(state));
    XSetLineAttributes(display, gc, line_width(), LineSolid, CapButt, JoinMiter);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    if (restore){
        XPoint points[5] = {
            tiny::x_point(px(mid_x-sc(2)), px(mid_y-sc(1))),
            tiny::x_point(px(mid_x+sc(2)), px(mid_y-sc(1))),
            tiny::x_point(px(mid_x+sc(2)), px(mid_y+sc(3))),
            tiny::x_point(px(mid_x-sc(2)), px(mid_y+sc(3))),
            tiny::x_point(px(mid_x-sc(2)), px(mid_y-sc(1)))};
        // maybe CoordModePrevious
        XDrawLines(display, window, gc, points, 5, CoordModeOrigin);
    } else {
        XPoint points[5] = {
            tiny::x_point(px(mid_x-sc(3)), px(mid_y-sc(3))),
            tiny::x_point(px(mid_x+sc(3)), px(mid_y-sc(3))),
            tiny::x_point(px(mid_x+sc(3)), px(mid_y+sc(3))),
            tiny::x_point(px(mid_x-sc(3)), px(mid_y+sc(3))),
            tiny::x_point(px(mid_x-sc(3)), px(mid_y-sc(3)))};
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

void MinimizeButton::on_expose(const XEvent &, void *)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    GC gc = XCreateGC(display, window, 0, nullptr);

    uint8_t state = get_theme_state();

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);

    XSetForeground(display, gc, get_style().get_fg(state));
    XSetLineAttributes(display, gc, line_width(), LineSolid, CapButt, JoinBevel);

    int mid_x = attrs.width/2;
    int mid_y = attrs.width/2;

    XDrawLine(display, window, gc,
        px(mid_x-sc(4)), px(mid_y+sc(3.5)), px(mid_x+sc(4)), px(mid_y+sc(3.5)));
    XFreeGC(display, gc);
}

} // namespace wm
