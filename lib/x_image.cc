#include "x_image.h"

#include <algorithm>

#include "x_util.h"

namespace tiny {

namespace {

constexpr int ARGB_DEPTH = 32;
constexpr int MAX_X_SIZE = 65535;

// Requests of this file may fail (no memory, a pixmap that is gone); it is
// better to return None than to let Xlib exit the window manager.
bool x_error_seen = false;

int note_x_error(::Display*, XErrorEvent*)
{
    x_error_seen = true;
    return 0;
}

class ErrorGuard {
  public:
    ErrorGuard(): previous(XSetErrorHandler(note_x_error))
    {
        x_error_seen = false;
    }
    ErrorGuard(const ErrorGuard&) = delete;
    ErrorGuard& operator=(const ErrorGuard&) = delete;
    ~ErrorGuard() { XSetErrorHandler(previous); }

    /// Wait for the errors of the requests sent so far.
    bool failed(::Display* display) const
    {
        XSync(display, False);
        return x_error_seen;
    }

  private:
    XErrorHandler previous;
};

bool has_depth(::Display* display, int depth)
{
    int count = 0;
    int* depths = XListDepths(display, DefaultScreen(display), &count);
    if (depths == nullptr) {
        return false;
    }
    const bool found =
            std::find(depths, depths + count, depth) != depths + count;
    XFree(depths);
    return found;
}

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
constexpr int HOST_BYTE_ORDER = MSBFirst;
#else
constexpr int HOST_BYTE_ORDER = LSBFirst;
#endif

} // namespace

Pixmap image_to_pixmap(::Display* display, ::Window root, const Image& image)
{
    if (!image.valid() || image.width > MAX_X_SIZE ||
        image.height > MAX_X_SIZE ||
        XRenderFindStandardFormat(display, PictStandardARGB32) == nullptr ||
        !has_depth(display, ARGB_DEPTH)) {
        return None;
    }

    // the pixels are in the byte order of this host: Xlib converts them
    XImage ximage = {};
    ximage.width = image.width;
    ximage.height = image.height;
    ximage.format = ZPixmap;
    // XPutImage only reads the data, the image does not own them
    ximage.data = const_cast<char*>(
            reinterpret_cast<const char*>(image.pixels.data()));
    ximage.byte_order = HOST_BYTE_ORDER;
    ximage.bitmap_unit = ARGB_DEPTH;
    ximage.bitmap_bit_order = BitmapBitOrder(display);
    ximage.bitmap_pad = ARGB_DEPTH;
    ximage.depth = ARGB_DEPTH;
    ximage.bytes_per_line = image.width * 4;
    ximage.bits_per_pixel = ARGB_DEPTH;
    ximage.red_mask = 0x00FF0000;
    ximage.green_mask = 0x0000FF00;
    ximage.blue_mask = 0x000000FF;
    if (XInitImage(&ximage) == 0) {
        return None;
    }

    const ErrorGuard guard;
    const Pixmap pixmap = XCreatePixmap(display, root, x_dim(image.width),
                                        x_dim(image.height), ARGB_DEPTH);
    GC gc = XCreateGC(display, pixmap, 0, nullptr);
    XPutImage(display, pixmap, gc, &ximage, 0, 0, 0, 0, x_dim(image.width),
              x_dim(image.height));
    XFreeGC(display, gc);
    if (guard.failed(display)) {
        XFreePixmap(display, pixmap);
        XSync(display, False);
        return None;
    }
    return pixmap;
}

Picture argb_picture(::Display* display, Pixmap pixmap)
{
    const XRenderPictFormat* format =
            XRenderFindStandardFormat(display, PictStandardARGB32);
    if (format == nullptr || pixmap == None) {
        return None;
    }

    const ErrorGuard guard;
    ::Window root;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    unsigned int border;
    unsigned int depth;
    const Status ok = XGetGeometry(display, pixmap, &root, &x, &y, &width,
                                   &height, &border, &depth);
    if (ok == 0 || guard.failed(display) || depth != ARGB_DEPTH) {
        return None;
    }

    const XRenderPictureAttributes attrs = {};
    const Picture picture =
            XRenderCreatePicture(display, pixmap, format, 0, &attrs);
    if (guard.failed(display)) {
        return None;
    }
    return picture;
}

} // namespace tiny
