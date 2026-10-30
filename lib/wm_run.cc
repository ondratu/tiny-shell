#include <unistd.h>
#include <cassert>

#include "spawn.h"
#include "wm_run.h"

namespace wm {

Entry::Entry(int w, int h):
    tiny::Widget(Widget::Type::Normal, w, h), xim(nullptr), xic(nullptr)
{
    name = "entry";
}

Entry::~Entry()
{
    disconnect(KeyPress);
    if (xic) {
        XDestroyIC(xic);
        xic = nullptr;
    }
    if (xim) {
        XCloseIM(xim);
        xim = nullptr;
    }
}

void Entry::realize(::Window parent_arg, int x, int y)
{
    tiny::Widget::realize(parent_arg, x, y);

    xim = XOpenIM(display, nullptr, nullptr, nullptr);
    xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, window, XNFocusWindow, window, NULL);
}

void Entry::set_events(long mask)
{
    tiny::Widget::set_events(mask | ExposureMask | KeyPressMask |
                             KeyReleaseMask | FocusChangeMask);

    connect(Expose, static_cast<tiny::event_signal_t>(&Entry::on_expose));

    connect(KeyPress,
            static_cast<tiny::event_signal_t>(&Entry::on_key_release));

    //connect(KeyRelease,
    //        static_cast<tiny::event_signal_t>(&Entry::on_key_release));

    connect(FocusIn, static_cast<tiny::event_signal_t>(&Entry::on_focus_in));
    connect(FocusOut, static_cast<tiny::event_signal_t>(&Entry::on_focus_out));
}

void Entry::on_key_release(const XEvent& e, void*)
{
    char buf[32] = "\0";
    KeySym keysym = NoSymbol;
    Status state;

    const int len = Xutf8LookupString(xic, const_cast<XKeyEvent*>(&e.xkey), buf,
                                      sizeof(buf) - 1, &keysym, &state);
    if (e.xkey.keycode == XKeysymToKeycode(display, XK_BackSpace)) {
        if (!text.empty()) {
            text.erase(text.size() - 1);
        } else {
            return;
        }
    } else if (e.xkey.keycode == XKeysymToKeycode(display, XK_Delete) ||
               e.xkey.keycode == XKeysymToKeycode(display, XK_Tab)) {
        // TODO: working with position
        // TODO: suggest like command line
    } else if (e.xkey.keycode == XKeysymToKeycode(display, XK_Return)) {
        tiny::spawn_shell(text);
        on_exit(this, e);
        return;
    } else if (e.xkey.keycode == XKeysymToKeycode(display, XK_Escape)) {
        on_exit(this, e);
        return;
    } else if (len > 0) {
        buf[len] = 0;
        text += buf;
    }

    on_expose(XEvent(), nullptr);
}

void Entry::on_expose(const XEvent&, void*)
{
    XftColor color;
    XftDraw* draw;
    XGlyphInfo extents;
    const int screen = 0; // XXX
    Visual* visual = XDefaultVisual(display, screen);
    const Colormap colormap = XDefaultColormap(display, screen);

    XftColorAllocName(display, visual, colormap,
                      tiny::theme.widget.get_xft_fg(get_theme_state()).data(),
                      &color);

    draw = XftDrawCreate(display, window, visual, colormap);

    XftTextExtentsUtf8(display, tiny::theme.widget.get_font(),
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);

    // Text is center normal center TODO: could be configurable
    const int x = (width - extents.width) / 2;
    const int y = (height + extents.height) / 2;

    XClearWindow(display, window);
    XftDrawStringUtf8(draw, &color, tiny::theme.widget.get_font(), x, y,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}

void Entry::on_focus_in(const XEvent&, void*)
{
    XSetICFocus(xic);
}

void Entry::on_focus_out(const XEvent&, void*)
{
    XUnsetICFocus(xic);
}

void Entry::set_focus()
{
    XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}

void Entry::clear()
{
    text.clear();
}

RunDialog::RunDialog():
    tiny::Popover(320, static_cast<int>(tiny::theme.wm_panel * 1.5)),
    entry(310, tiny::theme.wm_win_header)
{
    name = "run-dialog";
    entry.on_exit.connect(
            this, static_cast<tiny::object_signal_t>(&RunDialog::popover));
}

RunDialog::~RunDialog() = default;

void RunDialog::realize(::Window root, int, int)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    tiny::Popover::realize(root, attrs.width / 2 - width / 2,
                           attrs.height / 2 - height / 2);

    push_start(&entry);
}

void RunDialog::popup()
{
    map_all();
    XRaiseWindow(display, window);
    entry.set_focus();
}

void RunDialog::popover(Object*, const XEvent&, void*)
{
    entry.clear();
    unmap();
}

} // namespace wm
