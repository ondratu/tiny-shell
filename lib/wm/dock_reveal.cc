#include "wm/dock_reveal.h"

namespace tiny::wm {

DockReveal::DockReveal(int height): Widget(Type::Input, 1, height)
{
    name = "dock-reveal";
}

DockReveal::~DockReveal()
{
    if (event_done) {
        disconnect(EnterNotify);
    }
}

void DockReveal::set_events(long mask)
{
    Widget::set_events(mask | EnterWindowMask);
    connect(EnterNotify, [this](const XEvent& e) { on_enter_notify(e); });
}

void DockReveal::on_enter_notify(const XEvent& e)
{
    on_reach(this, e);
}

} // namespace tiny::wm
