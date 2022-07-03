#include <unistd.h>
#include <cassert>

#include "wm_run.h"

namespace wm {

Entry::Entry(uint32_t width, uint32_t height):
    tiny::Widget(Widget::Type::Normal, width, height),
    font(nullptr), screen(0), xim(0), xic(0)
{
    name = "entry";
}

Entry::~Entry()
{
    disconnect(KeyPress);
    if (font) {
        XftFontClose(display, font);
        font = nullptr;
    }
    if (xic) {
        XDestroyIC(xic);
        xic = 0;
    }
    if (xim) {
        XCloseIM(xim);
        xim = 0;
    }
}

void Entry::realize(Window parent, int x, int y)
{
    tiny::Widget::realize(parent, x, y);
    font = XftFontOpenName(display, screen, WM_WIN_HEADER_XFT_FONT);
    TINY_LOG("font %s is %x", WM_WIN_HEADER_XFT_FONT, font);

    xim = XOpenIM(display, NULL, NULL, NULL);
    TINY_LOG("xim: %d", xim);
    xic = XCreateIC(xim,
                    XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, window,
                    XNFocusWindow, window,
                    NULL);
    TINY_LOG("xic: %d", xic);
}

void Entry::set_events(long mask)
{
    tiny::Widget::set_events(mask|ExposureMask|KeyPressMask|KeyReleaseMask|FocusChangeMask);

    connect(Expose,
            static_cast<tiny::event_signal_t>(&Entry::on_expose));

    connect(KeyPress,
            static_cast<tiny::event_signal_t>(&Entry::on_key_release));

    //connect(KeyRelease,
    //        static_cast<tiny::event_signal_t>(&Entry::on_key_release));

    connect(FocusIn,
            static_cast<tiny::event_signal_t>(&Entry::on_focus_in));
    connect(FocusOut,
            static_cast<tiny::event_signal_t>(&Entry::on_focus_out));
}

void Entry::on_key_release(const XEvent& e, void* data)
{
    char buf[32] = "\0";
    KeySym keysym = NoSymbol;
    //XComposeStatus comp;
    Status state;

    //int len = XLookupString(const_cast<XKeyEvent*>(&e.xkey),
    //                        buf, 16, &keysym, &comp);
    //int len = XmbLookupString(xic¸, const_cast<XKeyEvent*>(&e.xkey),
    //                          buf, 16, &keysym, &state);
    int len = Xutf8LookupString(xic, const_cast<XKeyEvent*>(&e.xkey),
                                buf, sizeof(buf) - 1, &keysym, &state);
    TINY_LOG("state: %d", state);
    if (state == XBufferOverflow) {
        TINY_LOG("\t XBufferOverflow");
    }
    if (state == XLookupChars) {
        TINY_LOG("\t XLookupChars");
    }
    if (state == XLookupBoth) {
        TINY_LOG("\t XLookupBoth");
    }
    if (state == XLookupKeySym) {
        TINY_LOG("\t XLookupKeySym");
    }
    if (state == XLookupNone) {
        TINY_LOG("\t XLookupNone");
    }

    TINY_LOG("read string len: %d: %s", len, buf);
    if (e.xkey.keycode == XKeysymToKeycode(display, XK_BackSpace)) {
        if (text.size()){
            text.erase(text.size()-1);
        } else {
            return;
        }
    } else if (e.xkey.keycode == XKeysymToKeycode(display, XK_Delete)) {
        // TODO: working with position
    } else if (e.xkey.keycode == XKeysymToKeycode(display, XK_Tab)) {
        // TODO: suggest like command line
    } else if (e.xkey.keycode == XKeysymToKeycode(display, XK_Return)) {
        TINY_LOG("run %s", text.c_str());
        if (!fork()){
            int ret = putenv(XDisplayString(display));
            assert(ret != -1);
            std::string cmd = "exec ";
            cmd += text;
            ret = execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), NULL);
            exit(ret);
        } else {
            on_exit(this, e);
        }
        TINY_LOG("done");
        return;
    } else if (e.xkey.keycode == XKeysymToKeycode(display, XK_Escape)) {
        on_exit(this, e);
        return;
    } else if (len > 0) {
        buf[len]=0;
        text += buf;
    }

    on_expose(XEvent(), nullptr);
}

void Entry::on_expose(const XEvent& e, void* data)
{
    XftColor color;
    XftDraw *draw;
    XGlyphInfo extents;
    Visual *visual = XDefaultVisual(display, screen);
    Colormap colormap = XDefaultColormap(display, screen);

    XftColorAllocName(display, visual, colormap,
                XFT_WHITE_COLOR, &color);

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

void Entry::on_focus_in(const XEvent &e, void* data)
{
    TINY_LOG("XSetICFocus...");
    XSetICFocus(xic);
}

void Entry::on_focus_out(const XEvent &e, void* data)
{
    TINY_LOG("XUnsetICFocus...");
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
    tiny::Popover(320, WM_PANEL*1.5, 2, GRAY_COLOR, BLACK_COLOR),
    entry(310)
{
    name = "run-dialog";
    entry.on_exit.connect(this,
            static_cast<tiny::object_signal_t>(&RunDialog::popover));
}

RunDialog::~RunDialog()
{}

void RunDialog::realize(::Window root, int x, int y)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    tiny::Popover::realize(root, attrs.width/2-width/2, attrs.height/2-height/2);

    push_start(&entry);
}

void RunDialog::popup()
{
    map_all();
    XRaiseWindow(display, window);
    entry.set_focus();
}

void RunDialog::popover(Object *o, const XEvent &e, void* data)
{
    entry.clear();
    unmap();
}

} // namespace wm
