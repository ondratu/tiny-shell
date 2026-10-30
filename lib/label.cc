#include "label.h"

namespace tiny {

Label::Label(int w, int h, const std::string& text_arg):
    Widget(Type::Normal, w, h), text(text_arg)
{
    name = "label";
}

Label::~Label()
{
    if (event_done) {
        disconnect(Expose);
    }
}

void Label::set_events(long mask)
{
    Widget::set_events(mask | ExposureMask);

    connect(Expose, static_cast<event_signal_t>(&Label::on_expose));
}

void Label::set_text(const std::string& text_arg)
{
    text = text_arg;
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void Label::on_expose(const XEvent&, void*)
{
    const int screen = XDefaultScreen(display);
    Visual* visual = XDefaultVisual(display, screen);
    const Colormap colormap = XDefaultColormap(display, screen);
    const uint8_t state = get_theme_state();
    XftFont* font = get_style().get_font();

    XftColor color;
    XftColorAllocName(display, visual, colormap,
                      get_style().get_xft_fg(state).data(), &color);
    XftDraw* draw = XftDrawCreate(display, window, visual, colormap);

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, font, get_padding(),
                      (height + font->ascent - font->descent) / 2,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

} // namespace tiny
