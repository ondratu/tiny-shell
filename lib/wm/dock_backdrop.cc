#include "wm/dock_backdrop.h"

#include <algorithm>

#include "x_util.h"
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>

namespace tiny::wm {

namespace {

int ignore_error(::Display*, XErrorEvent*)
{
    return 0;
}

//! The size of a pixmap, false when it is gone (its owner has freed it)
bool pixmap_size(::Display* display, Pixmap pixmap, unsigned& width,
                 unsigned& height)
{
    ::Window root;
    int x;
    int y;
    unsigned border;
    unsigned depth;
    XSync(display, False); // the older errors are not ours to ignore
    XErrorHandler old = XSetErrorHandler(&ignore_error);
    const Status ok = XGetGeometry(display, pixmap, &root, &x, &y, &width,
                                   &height, &border, &depth);
    XSync(display, False);
    XSetErrorHandler(old);
    return ok != 0;
}

//! A channel (0 to 255) with the alpha (0 to 100 %) multiplied in, 16 bits
unsigned short premultiplied(uint32_t color, int shift, int opacity)
{
    return static_cast<unsigned short>(((color >> shift) & 0xFFu) * 257u *
                                       static_cast<unsigned>(opacity) / 100u);
}

} // namespace

Pixmap root_pixmap(::Display* display, ::Window root)
{
    const Atom atom = XInternAtom(display, "_XROOTPMAP_ID", True);
    if (atom == None) {
        return None;
    }
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    Pixmap pixmap = None;
    if (XGetWindowProperty(display, root, atom, 0, 1, False, XA_PIXMAP, &type,
                           &format, &count, &after, &data) == Success &&
        data != nullptr) {
        if (type == XA_PIXMAP && format == 32 && count == 1) {
            pixmap = *reinterpret_cast<Pixmap*>(data);
        }
        XFree(data);
    }
    unsigned width;
    unsigned height;
    return pixmap != None && pixmap_size(display, pixmap, width, height)
                   ? pixmap
                   : None;
}

Pixmap make_backdrop(::Display* display, ::Window root, const tiny::Rect& place,
                     int bar_top, uint32_t bar_color, int opacity,
                     uint32_t fallback)
{
    if (place.width <= 0 || place.height <= 0) {
        return None;
    }
    const int screen = XDefaultScreen(display);
    const Visual* visual = XDefaultVisual(display, screen);
    const XRenderPictFormat* format = XRenderFindVisualFormat(display, visual);
    if (format == nullptr) {
        return None;
    }
    const Pixmap pixmap = XCreatePixmap(
            display, root, x_dim(place.width), x_dim(place.height),
            static_cast<unsigned>(XDefaultDepth(display, screen)));
    GC gc = XCreateGC(display, pixmap, 0, nullptr);
    XSetForeground(display, gc, fallback);
    XFillRectangle(display, pixmap, gc, 0, 0, x_dim(place.width),
                   x_dim(place.height));
    const Pixmap wallpaper = root_pixmap(display, root); // it is not gone
    if (wallpaper != None) {
        // what is outside of the wallpaper keeps the fallback
        XCopyArea(display, wallpaper, pixmap, gc, place.x, place.y,
                  x_dim(place.width), x_dim(place.height), 0, 0);
    }
    XFreeGC(display, gc);

    const int top = std::clamp(bar_top, 0, place.height);
    if (top < place.height && opacity > 0) {
        const Picture picture =
                XRenderCreatePicture(display, pixmap, format, 0, nullptr);
        const XRenderColor color = {
                premultiplied(bar_color, 16, opacity),
                premultiplied(bar_color, 8, opacity),
                premultiplied(bar_color, 0, opacity),
                static_cast<unsigned short>(65535 * opacity / 100)};
        XRenderFillRectangle(display, PictOpOver, picture, &color, 0, top,
                             x_dim(place.width), x_dim(place.height - top));
        XRenderFreePicture(display, picture);
    }
    return pixmap;
}

std::vector<XRectangle> half_bar_shape(int width, int height,
                                       const std::vector<tiny::Rect>& parts)
{
    std::vector<XRectangle> shape;
    const auto rectangle = [](const tiny::Rect& r) {
        return XRectangle{static_cast<short>(r.x), static_cast<short>(r.y),
                          static_cast<unsigned short>(std::max(0, r.width)),
                          static_cast<unsigned short>(std::max(0, r.height))};
    };
    shape.push_back(rectangle({0, height / 2, width, height - height / 2}));
    for (const tiny::Rect& part : parts) {
        shape.push_back(rectangle(part));
    }
    return shape;
}

} // namespace tiny::wm
