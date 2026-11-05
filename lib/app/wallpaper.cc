#include "app/wallpaper.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>

#include "image/pixmap.h"
#include "x_util.h"

namespace tiny::app {

namespace {

//! Ignores the errors of the requests, like the one of a pixmap that is gone
int ignore_error(::Display* /*display*/, XErrorEvent* /*error*/)
{
    return 0;
}

//! The pixmap that a property of the root window names, None when it is not
//! there
Pixmap pixmap_property(::Display* display, ::Window root, const char* name)
{
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    Pixmap pixmap = None;
    if (XGetWindowProperty(display, root, XInternAtom(display, name, False), 0,
                           1, False, XA_PIXMAP, &type, &format, &count, &after,
                           &data) == Success &&
        data != nullptr) {
        if (type == XA_PIXMAP && format == 32 && count == 1) {
            pixmap = *reinterpret_cast<Pixmap*>(data);
        }
        XFree(data);
    }
    return pixmap;
}

void set_pixmap_property(::Display* display, ::Window root, const char* name,
                         Pixmap pixmap)
{
    XChangeProperty(display, root, XInternAtom(display, name, False), XA_PIXMAP,
                    32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&pixmap), 1);
}

} // namespace

image::Fit wallpaper_fit(const std::string& mode)
{
    if (mode == "center") {
        return image::Fit::Center;
    }
    return mode == "tile" ? image::Fit::Tile : image::Fit::Fill;
}

std::string expand_home(const std::string& path, const char* home)
{
    if (home == nullptr || *home == '\0' || path.empty() || path[0] != '~' ||
        (path.size() > 1 && path[1] != '/')) {
        return path;
    }
    return home + path.substr(1);
}

WallpaperRequest parse_wallpaper_args(const std::vector<std::string>& args)
{
    WallpaperRequest request;
    for (size_t i = 0; i < args.size() && request.error.empty(); ++i) {
        const std::string& arg = args[i];
        if (arg == "-h" || arg == "--help") {
            request.help = true;
        } else if (arg == "-m" || arg == "--mode") {
            if (i + 1 == args.size()) {
                request.error = arg + " needs a mode";
            } else if (!wm::valid_wallpaper_mode(args[++i])) {
                request.error = "the mode must be fill, center or tile";
            } else {
                request.mode = args[i] == "fill" ? "" : args[i];
            }
        } else if (arg.size() > 1 && arg[0] == '-') {
            request.error = "unknown option " + arg;
        } else if (!request.file.empty()) {
            request.error = "only one file";
        } else {
            request.file = arg;
        }
    }
    return request;
}

std::string wallpaper_usage(const char* program)
{
    return std::string("Usage: ") + program +
           " [-m MODE] [FILE]\n"
           "\n"
           "Draws a PNG or JPEG file as the wallpaper of the screen (the\n"
           "root window and _XROOTPMAP_ID). Without a FILE and a MODE it\n"
           "uses the [wallpaper] file and mode of tiny-shell.ini.\n"
           "\n"
           "  -m, --mode MODE  fill (default), center or tile\n"
           "  -h, --help       print this help\n";
}

bool set_root_pixmap(::Display* display, ::Window root,
                     const image::Image& picture)
{
    XWindowAttributes attrs;
    if (!picture.valid() || !XGetWindowAttributes(display, root, &attrs)) {
        return false;
    }
    const XRenderPictFormat* format =
            XRenderFindVisualFormat(display, attrs.visual);
    const Pixmap argb = image::create_pixmap(display, root, picture);
    if (format == nullptr || argb == None) {
        if (argb != None) {
            XFreePixmap(display, argb);
        }
        return false;
    }

    // the root has the depth of the screen: the picture of depth 32 is copied
    const Pixmap pixmap = XCreatePixmap(display, root, x_dim(picture.width),
                                        x_dim(picture.height),
                                        static_cast<unsigned>(attrs.depth));
    const Picture source = image::create_argb_picture(display, argb);
    const XRenderPictureAttributes none = {};
    const Picture target =
            XRenderCreatePicture(display, pixmap, format, 0, &none);
    if (source != None && target != None) {
        XRenderComposite(display, PictOpSrc, source, None, target, 0, 0, 0, 0,
                         0, 0, x_dim(picture.width), x_dim(picture.height));
    }
    const bool drawn = source != None && target != None;
    if (source != None) {
        XRenderFreePicture(display, source);
    }
    if (target != None) {
        XRenderFreePicture(display, target);
    }
    XFreePixmap(display, argb);
    if (!drawn) {
        XFreePixmap(display, pixmap);
        return false;
    }

    // two programs that run together (a quick series of resizes) must not
    // both take the same previous pixmap and leave the other's one behind
    XGrabServer(display);

    // the previous one is ours to free only when it was published as a pair
    const Pixmap old = pixmap_property(display, root, "_XROOTPMAP_ID");
    const bool old_is_ours =
            old != None &&
            old == pixmap_property(display, root, "ESETROOT_PMAP_ID");

    XSetWindowBackgroundPixmap(display, root, pixmap);
    XClearWindow(display, root);
    set_pixmap_property(display, root, "_XROOTPMAP_ID", pixmap);
    set_pixmap_property(display, root, "ESETROOT_PMAP_ID", pixmap);

    if (old_is_ours && old != pixmap) {
        // the pixmap is kept by the program that made it, which is gone; this
        // ends the retained resources of that client (the id may be stale)
        XSync(display, False);
        const auto previous = XSetErrorHandler(&ignore_error);
        XKillClient(display, old);
        XSync(display, False);
        XSetErrorHandler(previous);
    }
    XUngrabServer(display);
    XSync(display, False);
    return true;
}

bool apply_wallpaper(::Display* display, ::Window root, const std::string& file,
                     image::Fit fit, std::string* error)
{
    const auto fail = [error](const std::string& why) {
        if (error != nullptr) {
            *error = why;
        }
        return false;
    };
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display, root, &attrs)) {
        return fail("can't read the size of the screen");
    }
    const auto photo = image::load_photo(file);
    if (!photo) {
        return fail("can't read " + file + " (a PNG or JPEG is needed)");
    }
    const auto picture =
            image::fit_photo(*photo, attrs.width, attrs.height, fit);
    if (!picture || !set_root_pixmap(display, root, *picture)) {
        return fail("the server did not take the picture");
    }
    return true;
}

} // namespace tiny::app
