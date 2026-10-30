#include <X11/keysym.h>

#include <algorithm>

#include "entry.h"
#include "x_util.h"

namespace tiny {

Entry::Entry(int w, int h):
    Widget(Type::Normal, w, h), xim(nullptr), xic(nullptr)
{
    name = "entry";
}

Entry::~Entry()
{
    if (event_done) {
        disconnect(KeyPress);
        disconnect(Expose);
        disconnect(FocusIn);
        disconnect(FocusOut);
    }
    if (xic) {
        XDestroyIC(xic);
        xic = nullptr;
    }
    if (xim) {
        XCloseIM(xim);
        xim = nullptr;
    }
}

void Entry::realize(Window parent_arg, int x, int y)
{
    Widget::realize(parent_arg, x, y);

    xim = XOpenIM(display, nullptr, nullptr, nullptr);
    xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, window, XNFocusWindow, window, NULL);
}

void Entry::set_events(long mask)
{
    Widget::set_events(mask | ExposureMask | KeyPressMask | FocusChangeMask);

    connect(Expose, static_cast<event_signal_t>(&Entry::on_expose));
    connect(KeyPress, static_cast<event_signal_t>(&Entry::on_key_press));
    connect(FocusIn, static_cast<event_signal_t>(&Entry::on_focus_in));
    connect(FocusOut, static_cast<event_signal_t>(&Entry::on_focus_out));
}

void Entry::set_focus()
{
    XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}

void Entry::set_text(const std::string& text_arg)
{
    text = text_arg;
    redraw();
}

void Entry::clear()
{
    set_text("");
}

void Entry::on_key_press(const XEvent& e, void*)
{
    if (!xic) {
        return;
    }

    char buf[32] = "\0";
    KeySym keysym = NoSymbol;
    Status state;

    const int len = Xutf8LookupString(xic, const_cast<XKeyEvent*>(&e.xkey), buf,
                                      sizeof(buf) - 1, &keysym, &state);

    switch (keysym) {
        case XK_BackSpace:
            if (text.empty()) {
                return;
            }
            // remove the whole UTF-8 character, not only its last byte
            while ((static_cast<unsigned char>(text.back()) & 0xC0) == 0x80 &&
                   text.size() > 1) {
                text.pop_back();
            }
            text.pop_back();
            break;
        case XK_Return:
        case XK_KP_Enter:
            return on_activate(this, e);
        case XK_Escape:
            return on_cancel(this, e);
        case XK_Tab:
        case XK_Delete:
            // TODO: cursor position and completion
            return;
        default:
            // control characters (Ctrl+key, Tab, ...) are not text
            if (len <= 0 || static_cast<unsigned char>(buf[0]) < 0x20) {
                return;
            }
            buf[len] = 0;
            text += buf;
            break;
    }

    redraw();
    on_change(this, e);
}

void Entry::redraw()
{
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void Entry::on_expose(const XEvent&, void*)
{
    const int screen = XDefaultScreen(display);
    Visual* visual = XDefaultVisual(display, screen);
    const Colormap colormap = XDefaultColormap(display, screen);
    const uint8_t state = get_theme_state();
    const Style& style = get_style();
    XftFont* font = style.get_font();

    XftColor color;
    XftColorAllocName(display, visual, colormap, style.get_xft_fg(state).data(),
                      &color);
    XftDraw* draw = XftDrawCreate(display, window, visual, colormap);

    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);

    // a long text shows its end, so that the typed characters stay visible
    const int padding = get_padding();
    const int x = std::min(padding, width - padding - extents.xOff - 1);
    const int y = (height + font->ascent - font->descent) / 2;

    XSetWindowBackground(display, window, style.get_bg(state));
    XSetWindowBorder(display, window, style.get_br(state));
    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, font, x, y,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    if (is_focus) { // the cursor
        const int cursor_height = font->ascent + font->descent;
        XftDrawRect(draw, &color, x + extents.xOff,
                    (height - cursor_height) / 2, 1, x_uint(cursor_height));
    }

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

void Entry::on_focus_in(const XEvent&, void*)
{
    is_focus = true;
    if (xic) {
        XSetICFocus(xic);
    }
    redraw();
}

void Entry::on_focus_out(const XEvent&, void*)
{
    is_focus = false;
    if (xic) {
        XUnsetICFocus(xic);
    }
    redraw();
}

} // namespace tiny
