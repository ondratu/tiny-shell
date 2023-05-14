#include "wm_dock.h"

namespace wm {

DockButton::DockButton():
    tiny::Widget(tiny::theme.wm_icon,tiny::theme.wm_icon)
{
    name = "dock_button";
}

DockButton::~DockButton()
{
    XUngrabButton(display, Button1, AnyModifier, window);
    disconnect(ExposureMask);
    disconnect(EnterNotify);
    disconnect(LeaveNotify);
    disconnect(ButtonRelease);
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

void DockButton::on_button_release(const XEvent &e, void * data){
    // TODO: check if release is on window, if not do not call the handler
    if (on_click){
        on_click(this, e);
    }
}

void DockButton::on_expose(const XEvent& e, void* data)
{
    GC gc = XCreateGC(display, window, 0, nullptr);
    uint8_t state = get_theme_state();

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);

    XSetForeground(display, gc, get_style().get_fg(state));
    XSetLineAttributes(display, gc, 1, LineSolid, CapButt, JoinBevel);

    int offset = width/14;

    for (int y = 3; y <= 11; y+=4)
        for (int x = 3; x <= 11; x+=4)
    {
        XFillArc(display, window, gc,
                x*offset,
                y*offset,
                5, 5, 0, 360*64);
    }
    XFreeGC(display, gc);
}


Icon::Icon(Window* task):
    DockButton(),
    task(task)
{}

Icon::~Icon()
{}


void Icon::on_button_release(const XEvent& e, void* data){
    task->set_focus();
    DockButton::on_button_release(e, data);
}

void Icon::on_expose(const XEvent& e, void* data)
{
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

void Dock::add(Window* window)
{
    Icon* icon = new Icon(window);
    wm_task_icons[window] = icon;
    uint32_t size = icon->get_width()+tiny::theme.wm_dock_border;
    push_start(icon, tiny::theme.wm_dock_border, tiny::theme.wm_dock_border);
    resize(children.size()*size+tiny::theme.wm_dock_border, height);
    icon->map_all();
}

void Dock::remove(Window* window)
{
    Icon* icon = wm_task_icons[window];
    Box::remove(icon);
    wm_task_icons.erase(window);
    delete(icon);
    resize(children.size()*height, height);
}

void Dock::realize(::Window parent, int x, int y)
{
    tiny::Box::realize(parent, x, y);

    push_back(&icon, tiny::theme.wm_dock_border, tiny::theme.wm_dock_border);
}

void Dock::resize(uint32_t width, uint32_t height)
{
    Box::resize(width, height);

    if (is_realized){
        XWindowAttributes attrs;
        XGetWindowAttributes(display, parent, &attrs);
        XMoveWindow(display, window,
                attrs.width/2-width/2,
                attrs.height - height-tiny::theme.wm_dock_border);
    }
}


} // namespace
