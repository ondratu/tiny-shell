#include <X11/keysym.h>

#include <algorithm>
#include <array>

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

    // Without an input method the entry stays without input (xic is null)
    xim = XOpenIM(display, nullptr, nullptr, nullptr);
    if (xim) {
        xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, window, XNFocusWindow, window, NULL);
    }
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

std::string latin1_to_utf8(const char* bytes, int len)
{
    std::string out;
    for (int i = 0; i < len; ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::string Entry::lookup(const XKeyEvent& key, KeySym& keysym)
{
    auto* const event = const_cast<XKeyEvent*>(&key);
    if (!xic) { // no input method: Latin-1 only
        std::array<char, 32> buf{};
        const int len =
                XLookupString(event, buf.data(), buf.size(), &keysym, nullptr);
        return latin1_to_utf8(buf.data(), len);
    }

    std::string buf(32, '\0');
    Status state = 0;
    int len = Xutf8LookupString(xic, event, buf.data(),
                                static_cast<int>(buf.size()), &keysym, &state);
    if (state == XBufferOverflow) { // `len` is the needed size
        buf.assign(static_cast<size_t>(len), '\0');
        len = Xutf8LookupString(xic, event, buf.data(),
                                static_cast<int>(buf.size()), &keysym, &state);
    }
    if (state != XLookupChars && state != XLookupBoth) {
        return {}; // a key without characters; `len` means nothing
    }
    buf.resize(static_cast<size_t>(std::max(len, 0)));
    return buf;
}

void Entry::on_key_press(const XEvent& e, void*)
{
    KeySym keysym = NoSymbol;
    const std::string typed = lookup(e.xkey, keysym);

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
            // control characters (Ctrl+key, DEL, ...) are not text
            if (typed.empty() || static_cast<unsigned char>(typed[0]) < 0x20 ||
                typed[0] == 0x7F) {
                return;
            }
            text += typed;
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

void Entry::set_focus_state(bool focus, const XEvent& e)
{
    // With the pointer over the entry the server reports the focus of the
    // pointer window (NotifyPointer) and the focus of the entry twice: it is
    // not a change and must not redraw the entry.
    if (e.xfocus.detail == NotifyPointer || is_focus == focus) {
        return;
    }
    is_focus = focus;
    if (xic) {
        focus ? XSetICFocus(xic) : XUnsetICFocus(xic);
    }
    redraw();
}

void Entry::on_focus_in(const XEvent& e, void*)
{
    set_focus_state(true, e);
}

void Entry::on_focus_out(const XEvent& e, void*)
{
    set_focus_state(false, e);
}

} // namespace tiny
