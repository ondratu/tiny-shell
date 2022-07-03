#include <X11/cursorfont.h>

#include "wm_header.h"

namespace wm {

TitleBox::TitleBox(uint32_t width, uint32_t height):
    tiny::Widget(Widget::Type::Input, width, height)
{}

TitleBox::~TitleBox()
{
    XUngrabButton(display, Button1, AnyModifier, window);
    disconnect(ButtonPress);
    disconnect(ButtonRelease);
    disconnect(MotionNotify);
}

void TitleBox::set_events(long mask)
{
    tiny::Widget::set_events(mask);

    XGrabButton(            // Click to WM_header
        display, Button1, AnyModifier, window, true,
        ButtonPressMask | ButtonReleaseMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1));

    connect(ButtonPress,
            static_cast<tiny::event_signal_t>(&TitleBox::on_button_press));
    connect(ButtonRelease,
            static_cast<tiny::event_signal_t>(&TitleBox::on_button_release));
    connect(MotionNotify,
            static_cast<tiny::event_signal_t>(&TitleBox::on_motion_notify));
}

void TitleBox::on_button_press(const XEvent &e, void * data)
{
    XGrabPointer(
        display, window, false,
        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1), CurrentTime);
    on_drag_begin(this, e);
}

void TitleBox::on_button_release(const XEvent &e, void * data)
{
    XUngrabPointer(display, CurrentTime);
    on_drag_end(this, e);
}

void TitleBox::on_motion_notify(const XEvent &e, void * data)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent(  // skip penging motion events
                display, event.xmotion.window, MotionNotify, &event))
    {}
    on_drag_motion(this, event);
}


Header::Header(uint32_t width, uint32_t height):
    Box(Box::Type::Horizontal, width, height,
        0, 0x0, WM_WIN_BACKGROUND),
    font(nullptr),title_box(width, height),
    is_disable(false), screen(0)
{}

Header::~Header()
{
    XUngrabButton(display, Button1, AnyModifier, window);
    disconnect(Expose);
    if (font) {
        XftFontClose(display, font);
        font = nullptr;
    }
}

void Header::set_events(long mask){
    Box::set_events(ExposureMask|mask);

    connect(Expose,
            static_cast<tiny::event_signal_t>(&Header::on_expose));
}

void Header::realize(Window parent, int x, int y)
{
    Box::realize(parent, x, y);
    font = XftFontOpenName(display, screen, WM_WIN_HEADER_XFT_FONT);

    add(&title_box, 0, 0);
}

void Header::resize(uint32_t width, uint32_t height)
{
    Box::resize(width, height);
    title_box.resize(width, height);
    if (is_maped){
        on_expose(XEvent(), nullptr);
    }
}

void Header::set_title(const std::string &title)
{
    this->title = title;
    if (is_maped){
        on_expose(XEvent(), nullptr);
    }
}

void Header::set_disable(bool disable)
{
    is_disable = disable;
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void Header::on_expose(const XEvent &e, void * data)
{
    XftColor color;
    XftDraw *draw;
    XGlyphInfo extents;
    Visual *visual = XDefaultVisual(display, screen);
    Colormap colormap = XDefaultColormap(display, screen);

    if (is_disable){
        XftColorAllocName(display, visual, colormap,
                WM_WIN_HEADER_XFT_COLOR_DISABLE, &color);
    } else {
        XftColorAllocName(display, visual, colormap,
                WM_WIN_HEADER_XFT_COLOR_NORMAL, &color);
    }

    draw = XftDrawCreate(display, window, visual, colormap);

    XftTextExtentsUtf8(display, font,
            reinterpret_cast<const FcChar8*>(title.c_str()), title.size(),
            &extents);

    // Text is center normal center TODO: could be configurable
    int x = (width-extents.width)/2;
    int y = (height+extents.height)/2;

    if ((x + extents.width) > get_end_offset()){
        x = get_end_offset() - extents.width;
    }
    if (x < WM_WIN_HEADER_PADDING) {
        x = WM_WIN_HEADER_PADDING;
    }

    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, font, x, y,
            reinterpret_cast<const FcChar8*>(title.c_str()), title.size());

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);

    // bottom line
    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, WM_WIN_BORDER_COLOR);
    XSetLineAttributes(display, gc, 1, LineSolid, CapButt, JoinBevel);
    XDrawLine(display, window, gc, 0, height-1, width, height-1);
    XFreeGC(display, gc);
}

} // namespace wm
