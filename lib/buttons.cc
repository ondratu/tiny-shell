#include <X11/cursorfont.h>

#include "buttons.h"

namespace tiny {

Button::Button(uint32_t width, uint32_t height,
        uint32_t border, uint32_t border_color, uint32_t background):
    Widget(width, height, border, border_color, background),
    is_active(false)
{}

Button::~Button()
{
    XUngrabButton(display, Button1, AnyModifier, window);
    disconnect(EnterNotify);
    disconnect(LeaveNotify);
    disconnect(ButtonRelease);
}

void Button::set_events()
{
    XSelectInput(display, window, EnterWindowMask|LeaveWindowMask);
    XGrabButton(display, Button1, AnyModifier, window, false,
            ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync,
            None, XCreateFontCursor(display, XC_hand2));

    connect(EnterNotify,
            static_cast<event_signal_t>(&Button::on_enter_notify));
    connect(LeaveNotify,
            static_cast<event_signal_t>(&Button::on_leave_notify));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&Button::on_button_release));
}

void Button::on_enter_notify(const XEvent &e, void *){
    is_active = true;
}

void Button::on_leave_notify(const XEvent &e, void *){
    is_active = false;
}

void Button::on_button_release(const XEvent &e, void * data){
    // TODO: check if release is on window, if not do not call the handler
    if (on_click){
        on_click(this, e);
    }
}



LabelButton::LabelButton(uint32_t width, uint32_t height,
        const std::string &text, const std::string &font_name,
        uint32_t border, uint32_t border_color, uint32_t background):
    Button(width, height, border, border_color, background),
    text(text), font_name(font_name), normal_color(WIDGET_XFT_COLOR_NORMAL),
    active_color(WIDGET_XFT_COLOR_ACTIVE), screen(0)
{}

LabelButton::~LabelButton()
{}

void LabelButton::set_events()
{
    Button::set_events();
    XSelectInput(display, window, ExposureMask|EnterWindowMask|LeaveWindowMask);

    connect(Expose,
            static_cast<event_signal_t>(&LabelButton::on_expose));
}

void LabelButton::realize(Display * display, Window parent, int x, int y)
{
    Button::realize(display, parent, x, y);
    this->font = XftFontOpenName(display, screen, font_name.c_str());
}

void LabelButton::set_text(const std::string &text)
{
    this->text = text;
    if (font){
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
    Visual *visual = DefaultVisual(display, screen);
    Colormap colormap = DefaultColormap(display, screen);

    if (is_active){
        XftColorAllocName(display, visual, colormap, active_color.c_str(),
                &color);
    } else {
        XftColorAllocName(display, visual, colormap, normal_color.c_str(),
                &color);
    }

    draw = XftDrawCreate(display, window, visual, colormap);

    XftTextExtentsUtf8(display, font,
            reinterpret_cast<const FcChar8*>(text.c_str()), text.size(),
            &extents);

    // Text is center normal center TODO: could be configurable
    int x = (width-extents.width)/2;
    int y = (height+extents.height)/2;

    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, font, x, y,
            reinterpret_cast<const FcChar8*>(text.c_str()), text.size());

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

} // namespace tiny