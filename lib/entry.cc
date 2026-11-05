#include <X11/keysym.h>

#include <algorithm>

#include "entry.h"
#include "x_util.h"

namespace tiny {

Entry::Entry(int w, int h): Widget(Type::Normal, w, h)
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
        disconnect(ButtonPress);
        disconnect(SelectionNotify);
        disconnect(SelectionRequest);
        disconnect(SelectionClear);
    }
}

void Entry::realize(Window parent_arg, int x, int y)
{
    Widget::realize(parent_arg, x, y);

    input = std::make_unique<InputMethod>(display, window);
    clipboard = std::make_unique<Clipboard>(display, window);
}

void Entry::set_events(long mask)
{
    Widget::set_events(mask | ExposureMask | KeyPressMask | FocusChangeMask |
                       ButtonPressMask);

    connect(Expose, [this](const XEvent& e) { on_expose(e, nullptr); });
    connect(KeyPress, [this](const XEvent& e) { on_key_press(e, nullptr); });
    connect(FocusIn, [this](const XEvent& e) { on_focus_in(e, nullptr); });
    connect(FocusOut, [this](const XEvent& e) { on_focus_out(e, nullptr); });
    connect(ButtonPress,
            [this](const XEvent& e) { on_button_press(e, nullptr); });
    for (const int event :
         {SelectionNotify, SelectionRequest, SelectionClear}) {
        connect(static_cast<uint16_t>(event), [this](const XEvent& e) {
            if (clipboard) {
                clipboard->handle(e);
            }
        });
    }
}

void Entry::set_focus()
{
    XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}

void Entry::set_text(const std::string& text_arg)
{
    field.set_text(text_arg);
    redraw();
}

void Entry::clear()
{
    set_text("");
}

void Entry::paste(Clipboard::Which which, const XEvent& e)
{
    clipboard->request(
            which,
            [this, e](const std::string& text) {
                if (field.insert(single_line(text))) {
                    redraw();
                    on_change(this, e);
                }
            },
            e.xkey.time); // the key and the button events have it in one place
}

void Entry::on_key_press(const XEvent& e, void*)
{
    KeySym keysym = NoSymbol;
    const std::string typed = input->lookup(e.xkey, keysym);
    const bool control = (e.xkey.state & ControlMask) != 0;
    const bool shift = (e.xkey.state & ShiftMask) != 0;

    if (control && (keysym == XK_v || keysym == XK_V)) {
        return paste(Clipboard::Which::Clipboard, e);
    }
    if (shift && keysym == XK_Insert) {
        return paste(Clipboard::Which::Clipboard, e);
    }
    if (control && (keysym == XK_c || keysym == XK_C)) {
        // there is no selection inside of the entry: the whole text is copied;
        // an empty entry leaves the clipboard as it is
        if (!field.text().empty()) {
            clipboard->own(Clipboard::Which::Clipboard, field.text(),
                           e.xkey.time);
        }
        return;
    }

    bool edited = true;
    switch (keysym) {
        case XK_BackSpace:
            if (!field.backspace()) { // a whole UTF-8 character
                return;
            }
            break;
        case XK_Delete:
        case XK_KP_Delete:
            if (!field.erase()) {
                return;
            }
            break;
        case XK_Return:
        case XK_KP_Enter:
            return on_activate(this, e);
        case XK_Escape:
            return on_cancel(this, e);
        case XK_Left:
        case XK_Right:
            if (pass_horizontal) {
                return on_key(this, e);
            }
            edited = false;
            if (!(keysym == XK_Left ? field.left() : field.right())) {
                return;
            }
            break;
        case XK_Home:
        case XK_End:
            edited = false;
            if (!(keysym == XK_Home ? field.home() : field.end())) {
                return;
            }
            break;
        case XK_Up:
        case XK_Down:
        case XK_Page_Up:
        case XK_Page_Down:
            return on_key(this, e);
        case XK_Tab:
            // TODO: completion
            return;
        default:
            // control characters (Ctrl+key, DEL, ...) are not text
            if (typed.empty() || static_cast<unsigned char>(typed[0]) < 0x20 ||
                typed[0] == 0x7F) {
                return;
            }
            field.insert(typed);
            break;
    }

    redraw();
    if (edited) {
        on_change(this, e);
    }
}

void Entry::on_button_press(const XEvent& e, void*)
{
    if (e.xbutton.button == Button1) {
        XftFont* font = get_style().get_font();
        const int origin = text_origin(font);
        field.set_cursor(field.offset_at(e.xbutton.x - origin,
                                         [this, font](const std::string& text) {
                                             return text_width(font, text);
                                         }));
        redraw();
    } else if (e.xbutton.button == Button2) {
        paste(Clipboard::Which::Primary, e);
    }
}

int Entry::text_width(XftFont* font, const std::string& text) const
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);
    return extents.xOff;
}

int Entry::text_origin(XftFont* font) const
{
    // the text scrolls so that the cursor stays visible
    const int padding = get_padding();
    const int before = text_width(font, field.text().substr(0, field.cursor()));
    return std::min(padding, width - padding - before - 1);
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

    const std::string& text = field.text();
    const int x = text_origin(font);
    const int y = (height + font->ascent - font->descent) / 2;

    XSetWindowBackground(display, window, style.get_bg(state));
    XSetWindowBorder(display, window, style.get_br(state));
    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, font, x, y,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    if (is_focus) { // the cursor
        const int cursor_height = font->ascent + font->descent;
        const int before = text_width(font, text.substr(0, field.cursor()));
        XftDrawRect(draw, &color, x + before, (height - cursor_height) / 2, 1,
                    x_uint(cursor_height));
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
    input->set_focus(focus);
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
