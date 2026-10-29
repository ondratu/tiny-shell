#include <math.h>
#include <algorithm>

#include "wm_dock.h"
#include "object.h"
#include "wm_window.h"
#include <X11/Xlib.h>

namespace wm {

DockButton::DockButton():
    tiny::Widget(tiny::theme.wm_icon,tiny::theme.wm_icon)
{
    name = "dock_button";
}

DockButton::~DockButton()
{
    if (event_done) {
        XUngrabButton(display, Button1, AnyModifier, window);
        disconnect(Expose);
        disconnect(EnterNotify);
        disconnect(LeaveNotify);
        disconnect(ButtonRelease);
    }
}

void DockButton::set_events(long mask)
{
    tiny::Widget::set_events(ExposureMask|EnterWindowMask|LeaveWindowMask|mask);

    XGrabButton(display, Button1, AnyModifier, window, false,
            ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync,
            None, None);

    connect(EnterNotify,
            static_cast<tiny::event_signal_t>(&DockButton::on_enter_notify));
    connect(LeaveNotify,
            static_cast<tiny::event_signal_t>(&DockButton::on_leave_notify));
    connect(ButtonRelease,
            static_cast<tiny::event_signal_t>(&DockButton::on_button_release));
    connect(Expose,
            static_cast<tiny::event_signal_t>(&DockButton::on_expose));
}

void DockButton::on_enter_notify(const XEvent &e, void *){
    is_hover = true;
    on_expose(e, nullptr);
}

void DockButton::on_leave_notify(const XEvent &e, void *){
    is_hover = false;
    on_expose(e, nullptr);
}

void DockButton::on_button_release(const XEvent &e, void *){
    // TODO: check if release is on window, if not do not call the handler
    if (on_click){
        on_click(this, e);
    }
}

void DockButton::on_expose(const XEvent&, void*)
{
    GC gc = XCreateGC(display, window, 0, nullptr);
    uint8_t state = get_theme_state();

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);

    XSetForeground(display, gc, get_style().get_fg(state));
    XSetLineAttributes(display, gc, 1, LineSolid, CapButt, JoinBevel);

    int offset = width/14;
    unsigned int dot = static_cast<unsigned int>(
        std::max(1l, lround(5*tiny::theme.scale_factor)));

    for (int y = 3; y <= 11; y+=4)
        for (int x = 3; x <= 11; x+=4)
    {
        XFillArc(display, window, gc,
                x*offset,
                y*offset,
                dot, dot, 0, 360*64);
    }
    XFreeGC(display, gc);
}


Icon::Icon(Window* task_arg):
    DockButton(),
    task(task_arg)
{}

Icon::~Icon()
{}


void Icon::on_button_release(const XEvent& e, void* data){
    task->set_focus();
    DockButton::on_button_release(e, data);
}

void Icon::on_expose(const XEvent&, void*)
{
    is_selected = task->is_focused();

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetWindowBackground(display, window,
            get_style().get_bg(get_theme_state()));
    XClearWindow(display, window);

    Pixmap icon = task->get_icon();
    Pixmap icon_mask = task->get_icon_mask();
    if (icon){
        if (icon_mask){
            XGCValues gc_vals;
            gc_vals.clip_mask = icon_mask;
            XChangeGC(display, gc, GCClipMask, &gc_vals);
        }

        XCopyArea(display, icon, window, gc,
                        0, 0,
                        width, height,
                        0, 0);
    } else {
        // TODO: move to wm_window
        XSetForeground(display, gc, get_style().get_fg(get_theme_state()));
        XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinBevel);

        XDrawArc(display, window, gc,
                1, 1, width-3, height-3, 0, 360*64);
    }
    XFreeGC(display, gc);
}


Dock::Dock():
    tiny::Box(tiny::Box::Type::Horizontal,
            tiny::theme.wm_icon+tiny::theme.wm_dock_border*2,
            tiny::theme.wm_icon+tiny::theme.wm_dock_border*2)
{
    name = "wm_dock";
}

Dock::~Dock()
{
    for (auto it: wm_task_icons){
        delete(it.second);
    }
    wm_task_icons.clear();
}

void Dock::add(Window* window_arg)
{
    Icon* task_icon = new Icon(window_arg);
    wm_task_icons[window_arg] = task_icon;
    push_start(task_icon, tiny::theme.wm_dock_border, tiny::theme.wm_dock_border);
    resize_to_children();
    task_icon->map_all();
}

void Dock::remove(Window* window_arg)
{
    auto it = wm_task_icons.find(window_arg);
    if (it == wm_task_icons.end()){
        return;
    }
    Icon* task_icon = it->second;
    Box::remove(task_icon);
    wm_task_icons.erase(it);
    delete(task_icon);
    resize_to_children();
}

void Dock::resize_to_children()
{
    uint32_t size = tiny::theme.wm_icon+tiny::theme.wm_dock_border;
    resize(static_cast<uint32_t>(children.size())*size+tiny::theme.wm_dock_border, height);
}

void Dock::redraw(Window* window_arg, const XEvent& e)
{
    auto wm_task = wm_task_icons.find(window_arg);
    if (wm_task != wm_task_icons.end()){
        Icon* task_icon = wm_task_icons[window_arg];
        task_icon->redraw(e);
    }
}

void Dock::realize(::Window parent_arg, int x, int y)
{
    tiny::Box::realize(parent_arg, x, y);

    push_back(&icon, tiny::theme.wm_dock_border, tiny::theme.wm_dock_border);
}

void Dock::resize(uint32_t w, uint32_t h)
{
    Box::resize(w, h);

    if (is_realized){
        XWindowAttributes attrs;
        XGetWindowAttributes(display, parent, &attrs);
        XMoveWindow(display, window,
                attrs.width/2-w/2,
                attrs.height - h-tiny::theme.wm_dock_border);
    }
}


} // namespace
