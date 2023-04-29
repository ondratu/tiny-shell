#include <string>

#include "buttons.h"
#include "containers.h"

namespace tiny {

Button::Button(uint32_t width, uint32_t height):
    Widget(width, height)
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
    Widget::set_events(EnterWindowMask|LeaveWindowMask|mask);

    XGrabButton(display, Button1, AnyModifier, window, false,
            ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync,
            None, None);

    connect(EnterNotify,
            static_cast<event_signal_t>(&Button::on_enter_notify));
    connect(LeaveNotify,
            static_cast<event_signal_t>(&Button::on_leave_notify));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&Button::on_button_release));
}

void Button::on_enter_notify(const XEvent &e, void *){
    is_hover = true;
}

void Button::on_leave_notify(const XEvent &e, void *){
    is_hover = false;
}

void Button::on_button_release(const XEvent &e, void * data){
    // TODO: check if release is on window, if not do not call the handler
    if (on_click){
        on_click(this, e);
    }
}



LabelButton::LabelButton(uint32_t width, uint32_t height,
        const std::string &text):
    Button(width, height),
    text(text)
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
    Button::set_events(ExposureMask|mask);

    connect(Expose,
            static_cast<event_signal_t>(&LabelButton::on_expose));
}

void LabelButton::realize(Window parent, int x, int y)
{
    Button::realize(parent, x, y);
}

void LabelButton::set_text(const std::string &text)
{
    this->text = text;
    if (is_maped){
        on_expose(XEvent(), nullptr);
    }
}

void LabelButton::on_enter_notify(const XEvent &e, void * data)
{
    Button::on_enter_notify(e, data);
    on_expose(e, nullptr);
}

void LabelButton::on_leave_notify(const XEvent &e, void * data)
{
    Button::on_leave_notify(e, data);
    on_expose(e, nullptr);
}

void LabelButton::on_expose(const XEvent &e, void * data)
{
    XftColor color;
    XftDraw *draw;
    XGlyphInfo extents;
    const int screen = 0; // XXX
    Visual *visual = XDefaultVisual(display, screen);
    Colormap colormap = XDefaultColormap(display, screen);

    uint8_t state = get_theme_state();
    XftColorAllocName(display, visual, colormap,
                      tiny::theme.widget.get_xft_fg(state).data(), &color);

    draw = XftDrawCreate(display, window, visual, colormap);

    XftTextExtentsUtf8(display, tiny::theme.widget.get_font(),
            reinterpret_cast<const FcChar8*>(text.c_str()), text.size(),
            &extents);

    // Text is center normal center TODO: could be configurable
    int x = (width-extents.width)/2;
    int y = (height+extents.height)/2;

    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, tiny::theme.widget.get_font(), x, y,
            reinterpret_cast<const FcChar8*>(text.c_str()), text.size());

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}


MenuButton::MenuButton(uint32_t width, uint32_t height, const std::string &text):
    LabelButton(width, height, text)
{
    name = "menubutton";
}

MenuButton::~MenuButton()
{}

void MenuButton::set_events(long mask)
{
    on_click.connect(this,
            static_cast<object_signal_t>(&MenuButton::do_popup));
    LabelButton::set_events(mask);
}

void MenuButton::set_popover(Popover *popover)
{
    this->popover = popover;
}

void MenuButton::do_popup(Object *o, const XEvent &e, void *data)
{
    if (popover && !popover->get_maped()){
        int x = e.xbutton.x_root - e.xbutton.x + width/2;
        int y = e.xbutton.y_root - e.xbutton.y + height;
        popover->popup(x, y);       // center x, y under Button
    }
}

} // namespace tiny
