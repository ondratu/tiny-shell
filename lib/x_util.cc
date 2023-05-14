#include <X11/extensions/Xrender.h>
#include <algorithm>
#include <cmath>

#include "x_util.h"

namespace tiny {

void x_grab_key(::Display* display, int keycode, unsigned int modifiers,
        ::Window window)
{
    XGrabKey(display, keycode, modifiers,
             window, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, modifiers|Mod2Mask,  // Num Lock
             window, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, modifiers|LockMask,  // Caps Lock
             window, True, GrabModeAsync, GrabModeAsync);
    // Num Lock + Caps Lock
    XGrabKey(display, keycode, modifiers|Mod2Mask|LockMask,
             window, True, GrabModeAsync, GrabModeAsync);
}

void x_ungrab_key(::Display* display, int keycode, unsigned int modifiers,
        ::Window window)
{
    // Base without Num or Cups locks
    XUngrabKey(display, keycode, modifiers, window);
    // Num Lock
    XUngrabKey(display, keycode, modifiers|Mod2Mask, window);
    // Caps Lock
    XUngrabKey(display, keycode, modifiers|LockMask, window);
    // Num Lock + Caps Lock
    XUngrabKey(display, keycode, modifiers|Mod2Mask|LockMask, window);
}

Pixmap resize_pixmap (::Display* display, ::Window root, Pixmap origin,
        unsigned int o_width, unsigned int o_height,
        unsigned int t_width, unsigned int t_height,
        unsigned int depth)
{
    Pixmap target = XCreatePixmap(display, root, t_width, t_height,
            depth); // TODO: Screen instead of 0

    Visual *visual = XDefaultVisual(display, 0);    // Right screen!
    XRenderPictFormat* format;
    if (depth == 1) {
        format = XRenderFindStandardFormat(display, PictStandardA1);
    } else {
        format = XRenderFindVisualFormat(display, visual);
    }

    XRenderPictureAttributes attrs;
    Picture src = XRenderCreatePicture(display, origin, format, 0, &attrs);
    Picture dst = XRenderCreatePicture(display, target, format, 0, &attrs);

    double x_scale = (double)o_width / t_width;
    double y_scale = (double)o_height / t_height;
    double scale = std::max(x_scale, y_scale);

    XTransform transform = {{
        {XDoubleToFixed(scale), XDoubleToFixed(0), XDoubleToFixed(0)},
            {XDoubleToFixed(0), XDoubleToFixed(scale), XDoubleToFixed(0)},
            {XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(1.0)}
    }};
    XRenderSetPictureTransform(display, src, &transform);
    XRenderComposite(display, PictOpSrc, src, 0, dst,
            0, 0, 0, 0, 0, 0,   // TODO: shift after scale
            t_width, t_height);

    XRenderFreePicture(display, src);
    XRenderFreePicture(display, dst);

    return target;
}

} // namespace tiny
