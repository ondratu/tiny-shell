#include "theme.h"
#include "x_util.h"
#include "wm_size_label.h"

namespace wm {

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

void SizeLabel::center()
{
    XftFont* font = get_style().get_font();
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);

    const int padding = font->height;
    width = extents.xOff + 2 * padding;
    height = font->height + padding;
    XMoveResizeWindow(display, window, (area_width - width) / 2,
                      area_y + (area_height - height) / 2, tiny::x_dim(width),
                      tiny::x_dim(height));
}

void SizeLabel::set_text(const std::string& text_arg)
{
    text = text_arg;
    center();
    if (is_maped) {
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

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, font, (width - extents.width) / 2,
                      (height + extents.height) / 2,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

} // namespace wm
