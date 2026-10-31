#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include "image/image.h"

namespace tiny::image {

/**
 * @short Put the picture into a new pixmap of depth 32 (ARGB).
 *
 * The caller frees it with XFreePixmap. Returns None for an invalid or too big
 * (over 65535 pixels) picture, when the server has no pixmaps of depth 32 or no
 * ARGB32 format of Xrender, and when the server refuses the request.
 */
Pixmap create_pixmap(::Display* display, ::Window root, const Image& image);

/**
 * @short Xrender picture of a pixmap of depth 32.
 *
 * The caller frees it with XRenderFreePicture. Returns None for a pixmap of
 * another depth, a pixmap that does not exist and without ARGB32.
 */
Picture create_argb_picture(::Display* display, Pixmap pixmap);

Pixmap resize_pixmap(::Display* display, ::Window root, Pixmap origin,
                     unsigned int o_width, unsigned int o_height,
                     unsigned int t_width, unsigned int t_height,
                     unsigned int depth);

} // namespace tiny::image
