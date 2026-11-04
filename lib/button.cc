#include <string>

#include "button.h"
#include "container.h"

namespace tiny {

Button::Button(int w, int h): Widget(w, h)
{
    name = "button";
}

Button::~Button()
{
    if (event_done) {
        XUngrabButton(display, Button1, AnyModifier, window);
        disconnect(EnterNotify);
        disconnect(LeaveNotify);
        disconnect(ButtonRelease);
    }
}

void Button::set_events(long mask)
{
    Widget::set_events(EnterWindowMask | LeaveWindowMask | mask);

    XGrabButton(display, Button1, AnyModifier, window, false, ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None);

    connect(EnterNotify,
            [this](const XEvent& e) { on_enter_notify(e, nullptr); });
    connect(LeaveNotify,
            [this](const XEvent& e) { on_leave_notify(e, nullptr); });
    connect(ButtonRelease,
            [this](const XEvent& e) { on_button_release(e, nullptr); });
}

void Button::on_enter_notify(const XEvent& e, void* data)
{
    is_hover = true;
    on_enter(this, e, data);
}

void Button::on_leave_notify(const XEvent& e, void* data)
{
    is_hover = false;
    on_leave(this, e, data);
}

void Button::on_button_release(const XEvent& e, void*)
{
    if (on_click && is_hover) {
        on_click(this, e);
    }
}

LabelButton::LabelButton(int w, int h, const std::string& text_arg):
    Button(w, h), text(text_arg)
{
    name = "labelbutton";
}

LabelButton::~LabelButton()
{
    if (event_done) {
        disconnect(Expose);
    }
}

void LabelButton::set_events(long mask)
{
    Button::set_events(ExposureMask | mask);

    connect(Expose, [this](const XEvent& e) { on_expose(e, nullptr); });
}

void LabelButton::set_text(const std::string& text_arg)
{
    this->text = text_arg;
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

int LabelButton::text_width() const
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, tiny::theme.widget.get_font(),
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);
    return extents.xOff;
}

void LabelButton::on_enter_notify(const XEvent& e, void* data)
{
    Button::on_enter_notify(e, data);
    on_expose(e, nullptr);
}

void LabelButton::on_leave_notify(const XEvent& e, void* data)
{
    Button::on_leave_notify(e, data);
    on_expose(e, nullptr);
}

void LabelButton::on_expose(const XEvent&, void*)
{
    XftColor color;
    XftDraw* draw;
    XGlyphInfo extents;
    const int screen = 0; // XXX
    Visual* visual = XDefaultVisual(display, screen);
    const Colormap colormap = XDefaultColormap(display, screen);

    const uint8_t state = get_theme_state();
    XftColorAllocName(display, visual, colormap,
                      tiny::theme.widget.get_xft_fg(state).data(), &color);

    draw = XftDrawCreate(display, window, visual, colormap);

    XftTextExtentsUtf8(display, tiny::theme.widget.get_font(),
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);

    // Text is centered unless the owner wants it from the left
    const int x =
            text_padding >= 0 ? text_padding : (width - extents.width) / 2;
    const int y = (height + extents.height) / 2;

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, tiny::theme.widget.get_font(), x, y,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

MenuButton::MenuButton(int w, int h, const std::string& text_arg):
    LabelButton(w, h, text_arg)
{
    name = "menubutton";
}

MenuButton::~MenuButton() = default;

void MenuButton::set_events(long mask)
{
    on_click.connect([this](tiny::Object* o, const XEvent& e, void* d) {
        do_popup(o, e, d);
    });
    LabelButton::set_events(mask);
}

void MenuButton::set_popover(Popover* popover_arg)
{
    this->popover = popover_arg;
}

void MenuButton::do_popup(Object*, const XEvent& e, void*)
{
    if (popover && !popover->get_maped()) {
        const int x = e.xbutton.x_root - e.xbutton.x + width / 2;
        const int y = e.xbutton.y_root - e.xbutton.y + height;
        popover->popup(x, y); // center x, y under Button
    }
}

} // namespace tiny
