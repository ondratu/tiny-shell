#pragma once

// gtest must be included before Xlib: X.h defines the None macro
#include <gtest/gtest.h>

#include <vector>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "display.h"
#include "object.h"
#include "widget.h"
#include "x_util.h"

namespace tiny {
extern Handlers handlers;
}

inline XEvent make_event(int type)
{
    XEvent e = {};
    e.type = type;
    return e;
}

inline ::Window root_window()
{
    return XDefaultRootWindow(tiny::get_display());
}

// Calls the handler registered for `type` on `window`, like the main loop.
inline void send(::Window window, const XEvent& e)
{
    tiny::handlers.call_hanlder({e.type, window}, e);
}

inline void send(const tiny::Widget& w, int type)
{
    send(w.get_window(), make_event(type));
}

// Remembers the last signal it received.
class Recorder : public tiny::Object {
  public:
    void on_signal(tiny::Object* owner, const XEvent& e, void* data)
    {
        ++calls;
        last_owner = owner;
        last_event = e;
        last_data = data;
    }

    tiny::object_signal_t slot()
    {
        return static_cast<tiny::object_signal_t>(&Recorder::on_signal);
    }

    int calls = 0;
    tiny::Object* last_owner = nullptr;
    XEvent last_event = {};
    void* last_data = nullptr;
};

inline XWindowAttributes attributes_of(::Window w)
{
    XSync(tiny::get_display(), False);
    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), w, &attrs);
    return attrs;
}

inline XWindowAttributes attributes_of(const tiny::Widget& w)
{
    return attributes_of(w.get_window());
}

// Windows directly inside `parent`.
inline std::vector<::Window> children_of(::Window parent)
{
    XSync(tiny::get_display(), False);
    ::Window rv_root;
    ::Window rv_parent;
    ::Window* children = nullptr;
    unsigned int count = 0;
    XQueryTree(tiny::get_display(), parent, &rv_root, &rv_parent, &children,
               &count);
    std::vector<::Window> rv(children, children + count);
    if (children) {
        XFree(children);
    }
    return rv;
}

// Child of `parent` placed at x, y (0 when there is none).
inline ::Window child_at(::Window parent, int x, int y)
{
    for (const ::Window w : children_of(parent)) {
        const XWindowAttributes attrs = attributes_of(w);
        if (attrs.x == x && attrs.y == y) {
            return w;
        }
    }
    return 0;
}

// Contents of an InputOutput window or pixmap.
class Image {
  public:
    Image(::Drawable d, int w, int h): width(w), height(h)
    {
        XSync(tiny::get_display(), False);
        XImage* img = XGetImage(tiny::get_display(), d, 0, 0, tiny::x_uint(w),
                                tiny::x_uint(h), AllPlanes, ZPixmap);
        pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                pixels[index(x, y)] =
                        static_cast<uint32_t>(XGetPixel(img, x, y)) & 0xFFFFFF;
            }
        }
        XDestroyImage(img);
    }

    uint32_t at(int x, int y) const { return pixels[index(x, y)]; }

    // Number of pixels different from `bg` in the rows [0, rows).
    unsigned int count_not(uint32_t bg, int rows) const
    {
        unsigned int n = 0;
        for (int y = 0; y < rows && y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                n += at(x, y) != bg;
            }
        }
        return n;
    }

    bool operator==(const Image& o) const
    {
        return width == o.width && height == o.height && pixels == o.pixels;
    }

    int width;
    int height;
    std::vector<uint32_t> pixels;

  private:
    size_t index(int x, int y) const
    {
        return static_cast<size_t>(y) * static_cast<size_t>(width) +
               static_cast<size_t>(x);
    }
};

// Counts X protocol errors while it lives (an unhandled one kills the
// process in Xlib's default handler).
class XErrors {
  public:
    XErrors()
    {
        count = 0;
        old = XSetErrorHandler(handler);
    }

    ~XErrors()
    {
        XSync(tiny::get_display(), False);
        XSetErrorHandler(old);
    }

    int errors() const
    {
        XSync(tiny::get_display(), False);
        return count;
    }

  private:
    static int handler(::Display*, XErrorEvent*)
    {
        ++count;
        return 0;
    }

    static int count;
    XErrorHandler old;
};

inline int XErrors::count = 0;

// Gives theme.widget a font for the lifetime of the object (drawing text
// crashes without one) and restores the original style afterwards.
class WidgetFont {
  public:
    WidgetFont(): original(tiny::theme.widget), with_font(original)
    {
        with_font.init();
        tiny::theme.widget = with_font;
    }

    ~WidgetFont() { tiny::theme.widget = original; }

  private:
    tiny::Style original;
    tiny::Style with_font;
};

// Delivers the pending events of the manager's connection to the registered
// handlers (the main loop of the window manager does the same).
inline void pump_events()
{
    XSync(tiny::get_display(), False);
    XEvent e;
    while (XPending(tiny::get_display())) {
        XNextEvent(tiny::get_display(), &e);
        const std::pair<uint16_t, ::Window> key(e.type,
                                                tiny::window_of_event(e));
        if (tiny::handlers.count(key)) {
            tiny::handlers.call_hanlder(key, e);
        }
    }
}

// Same as WidgetFont for theme.wm_header.
class HeaderFont {
  public:
    HeaderFont(): original(tiny::theme.wm_header), with_font(original)
    {
        with_font.init();
        tiny::theme.wm_header = with_font;
    }

    ~HeaderFont() { tiny::theme.wm_header = original; }

  private:
    tiny::WMHeaderStyle original;
    tiny::WMHeaderStyle with_font;
};
