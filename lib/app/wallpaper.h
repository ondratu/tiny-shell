#pragma once

#include <string>
#include <vector>

#include <X11/X.h>
#include <X11/Xlib.h>

#include "image/photo.h"
#include "wm/config.h"

namespace tiny::app {

/// How a `[wallpaper] mode` covers the screen: "center" and "tile", else fill.
image::Fit wallpaper_fit(const std::string& mode);

/// The path with a leading `~/` (or only `~`) replaced by `home`; the path
/// as it is for another start or when `home` is null or empty.
std::string expand_home(const std::string& path, const char* home);

//! What the command line of `tiny-wallpaper` asks for
struct WallpaperRequest {
    std::string file; //!< "" is the one of the configuration
    std::string mode; //!< "" is the one of the configuration
    bool help = false;
    std::string error; //!< not empty: the arguments are wrong
};

/// `[-m fill|center|tile] [FILE]`, or `-h`.
WallpaperRequest parse_wallpaper_args(const std::vector<std::string>& args);

std::string wallpaper_usage(const char* program);

/**
 * @short Make the picture the background of the root window.
 *
 * The pixmap becomes the background of the root window and is published in
 * `_XROOTPMAP_ID` and `ESETROOT_PMAP_ID`, as `Esetroot` does, so that a
 * transparent terminal can copy from it. The pixmap of the wallpaper before
 * it is freed when it was published the same way. The pixmap lives as long as
 * the connection, so the program has to call `XSetCloseDownMode` with
 * `RetainPermanent` before it closes the connection (after a success only).
 * Returns false (and nothing changes) when the picture does not fit a pixmap or the server refuses it.
 *
 * Freeing the old pixmap ends the client that made it, so a connection must
 * set the wallpaper only once (a program that exits after that, like
 * `tiny-wallpaper`, does).
 */
bool set_root_pixmap(::Display* display, ::Window root,
                     const image::Image& picture);

/**
 * @short Load the photo and make it the wallpaper of the screen.
 *
 * The photo is fitted to the size of the root window now. Returns false and
 * says why in `error` when the file is not a PNG or JPEG that can be read or
 * the server refuses the picture.
 */
bool apply_wallpaper(::Display* display, ::Window root, const std::string& file,
                     image::Fit fit, std::string* error);

} // namespace tiny::app
