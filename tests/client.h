#pragma once

#include "helpers.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <string>
#include <vector>

inline tiny::Display& dsp()
{
    return tiny::get_display();
}

// A client application: a window created over its own X connection, like a
// real program managed by the window manager.
class Client {
  public:
    Client(int x = 10, int y = 20, unsigned width = 200, unsigned height = 100)
    {
        display = XOpenDisplay(nullptr);
        window = XCreateSimpleWindow(display, DefaultRootWindow(display), x, y,
                                     width, height, 0, 0, 0);
        sync();
    }

    ~Client()
    {
        XSync(dsp(), False); // requests for resources about to disappear
        if (display) {
            XCloseDisplay(display);
        }
    }

    // After the server killed the connection it must not be closed cleanly.
    void abandon() { display = nullptr; }

    // Both connections: the manager's requests are buffered too.
    void sync()
    {
        XSync(dsp(), False);
        XSync(display, False);
    }

    void map()
    {
        XMapWindow(display, window);
        sync();
    }

    void set_atoms(Atom property, const std::vector<Atom>& atoms)
    {
        XChangeProperty(display, window, property, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(atoms.data()),
                        static_cast<int>(atoms.size()));
        sync();
    }

    void set_cardinals(Atom property, const std::vector<long>& values)
    {
        XChangeProperty(display, window, property, XA_CARDINAL, 32,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(values.data()),
                        static_cast<int>(values.size()));
        sync();
    }

    void set_utf8(Atom property, const std::string& text)
    {
        XChangeProperty(display, window, property, dsp().UTF8_STRING, 8,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(text.c_str()),
                        static_cast<int>(text.size()));
        sync();
    }

    void set_title(const std::string& title)
    {
        XStoreName(display, window, title.c_str());
        sync();
    }

    void set_state(const std::vector<Atom>& atoms)
    {
        set_atoms(dsp()._NET_WM_STATE, atoms);
    }

    void set_protocols(const std::vector<Atom>& atoms)
    {
        XSetWMProtocols(display, window, const_cast<Atom*>(atoms.data()),
                        static_cast<int>(atoms.size()));
        sync();
    }

    void set_motif(unsigned long flags, unsigned long functions,
                   unsigned long decorations)
    {
        long hints[5] = {static_cast<long>(flags), static_cast<long>(functions),
                         static_cast<long>(decorations), 0, 0};
        XChangeProperty(display, window, dsp()._MOTIF_WM_HINTS,
                        dsp()._MOTIF_WM_HINTS, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(hints), 5);
        sync();
    }

    void set_size_hints(long flags, int min_w, int min_h, int max_w, int max_h)
    {
        XSizeHints hints = {};
        hints.flags = flags;
        hints.min_width = min_w;
        hints.min_height = min_h;
        hints.max_width = max_w;
        hints.max_height = max_h;
        XSetWMNormalHints(display, window, &hints);
        sync();
    }

    // Pixmap filled with a color, in the depth of the default screen.
    Pixmap pixmap(int size, unsigned long color)
    {
        Pixmap p = XCreatePixmap(display, DefaultRootWindow(display),
                                 tiny::x_uint(size), tiny::x_uint(size),
                                 tiny::x_uint(DefaultDepth(display, 0)));
        GC gc = XCreateGC(display, p, 0, nullptr);
        XSetForeground(display, gc, color);
        XFillRectangle(display, p, gc, 0, 0, tiny::x_uint(size),
                       tiny::x_uint(size));
        XFreeGC(display, gc);
        return p;
    }

    void set_icon_pixmap(int size, bool with_mask)
    {
        XWMHints hints = {};
        hints.flags = IconPixmapHint;
        hints.icon_pixmap = pixmap(size, 0xFF0000);
        if (with_mask) {
            hints.flags |= IconMaskHint;
            hints.icon_mask =
                    XCreatePixmap(display, DefaultRootWindow(display),
                                  tiny::x_uint(size), tiny::x_uint(size), 1);
        }
        XSetWMHints(display, window, &hints);
        sync();
    }

    // _NET_WM_ICON: width, height and ARGB pixels in 64 bit words
    void set_net_icon(int width, int height)
    {
        std::vector<long> data = {static_cast<long>(width),
                                  static_cast<long>(height)};
        data.insert(data.end(),
                    static_cast<size_t>(width) * static_cast<size_t>(height),
                    0xFFFF0000L);
        set_cardinals(dsp()._NET_WM_ICON, data);
    }

    // Returns true and fills `out` when a ClientMessage is waiting.
    bool message(XEvent& out)
    {
        XSync(dsp(), False); // flush what the manager has sent
        sync();
        return XCheckTypedWindowEvent(display, window, ClientMessage, &out);
    }

    std::vector<Atom> state_property()
    {
        sync();
        Atom type;
        int format;
        unsigned long count;
        unsigned long left;
        unsigned char* data = nullptr;
        std::vector<Atom> rv;
        XGetWindowProperty(display, window, dsp()._NET_WM_STATE, 0, 64, False,
                           XA_ATOM, &type, &format, &count, &left, &data);
        if (data) {
            Atom* atoms = reinterpret_cast<Atom*>(data);
            rv.assign(atoms, atoms + count);
            XFree(data);
        }
        return rv;
    }

    bool state_has(Atom atom)
    {
        auto states = state_property();
        return std::find(states.begin(), states.end(), atom) != states.end();
    }

    XWindowAttributes attrs()
    {
        sync();
        XWindowAttributes a;
        XGetWindowAttributes(display, window, &a);
        return a;
    }

    ::Display* display;
    ::Window window;
};
