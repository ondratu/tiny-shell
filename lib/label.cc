#include "label.h"

#include <fontconfig/fontconfig.h>

#include <cmath>

#include "display.h"

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
    close_font();
}

void Label::close_font()
{
    // the font of a display that is closed is closed with it
    if (own_font != nullptr && Display::live_serial() == own_font_serial) {
        XftFontClose(display, own_font);
    }
    own_font = nullptr;
}

void Label::set_align(Align value)
{
    align = value;
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void Label::set_font_scale(double factor)
{
    close_font();
    if (factor != 1) {
        // the matched pattern of the font of the style, with another size
        const XftFont* base = get_style().get_font();
        FcPattern* pattern = FcPatternDuplicate(base->pattern);
        double pixels = 0;
        if (pattern != nullptr &&
            FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &pixels) ==
                    FcResultMatch) {
            FcPatternDel(pattern, FC_PIXEL_SIZE);
            FcPatternDel(pattern, FC_SIZE);
            FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixels * factor);
            own_font =
                    XftFontOpenPattern(display, pattern); // takes the pattern
            own_font_serial = Display::live_serial();
            pattern = nullptr;
        }
        if (pattern != nullptr) {
            FcPatternDestroy(pattern);
        }
    }
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

XftFont* Label::current_font() const
{
    return own_font != nullptr ? own_font : get_style().get_font();
}

int Label::natural_height() const
{
    const XftFont* font = current_font();
    return font->ascent + font->descent + 2 * get_padding();
}

int Label::text_width() const
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, current_font(),
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);
    return extents.xOff;
}

void Label::set_events(long mask)
{
    Widget::set_events(mask | ExposureMask);

    connect(Expose, [this](const XEvent& e) { on_expose(e, nullptr); });
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
    XftFont* font = current_font();

    XftColor color;
    XftColorAllocName(display, visual, colormap,
                      get_style().get_xft_fg(state).data(), &color);
    XftDraw* draw = XftDrawCreate(display, window, visual, colormap);

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);
    const int x =
            align == Align::Center ? (width - text_width()) / 2 : get_padding();
    XftDrawStringUtf8(draw, &color, font, x,
                      (height + font->ascent - font->descent) / 2,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

} // namespace tiny
