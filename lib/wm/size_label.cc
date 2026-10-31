#include "theme.h"
#include "x_util.h"
#include "wm/size_label.h"

namespace tiny::wm {

SizeLabel::SizeLabel(int w, int h): tiny::Widget(w, h)
{
    name = "wm_size_label";
}

SizeLabel::~SizeLabel()
{
    if (event_done) {
        disconnect(Expose);
    }
}

void SizeLabel::set_events(long mask)
{
    Widget::set_events(ExposureMask | mask);

    connect(Expose, static_cast<tiny::event_signal_t>(&SizeLabel::on_expose));
}

void SizeLabel::set_area(int w, int h, int y)
{
    area_width = w;
    area_height = h;
    area_y = y;
    center();
}

bool SizeLabel::center()
{
    XftFont* font = get_style().get_font();
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);

    const int padding = font->height;
    const int new_width = extents.xOff + 2 * padding;
    const int new_height = font->height + padding;
    const bool resized = new_width != width || new_height != height;

    // width and height are the same until ConfigureNotify, so remember them
    // here: the next call must not resize the window again
    width = new_width;
    height = new_height;
    XMoveResizeWindow(display, window, (area_width - width) / 2,
                      area_y + (area_height - height) / 2, tiny::x_dim(width),
                      tiny::x_dim(height));
    return resized;
}

void SizeLabel::set_text(const std::string& text_arg)
{
    if (text_arg == text) {
        return;
    }
    text = text_arg;
    // The server exposes the whole window when it is resized and the Expose
    // draws the text, so draw here only when the size stays
    if (!center() && is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void SizeLabel::on_expose(const XEvent&, void*)
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

    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);

    // the baseline does not depend on the glyphs, the text does not jump
    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, font, (width - extents.width) / 2,
                      (height + font->ascent - font->descent) / 2,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

} // namespace tiny::wm
