// tiny-wallpaper: draws a PNG or JPEG file as the wallpaper of the screen.
//
// The picture becomes the background of the root window and is published in
// _XROOTPMAP_ID (like Esetroot does), so a transparent terminal can show it.
// The pixmap has to stay when this program ends: the connection is closed
// with RetainPermanent, and the next run frees the pixmap of this one.
//
// tiny-shell starts it when it starts and when the screen changes its size,
// and tiny-settings when it saves. What the arguments do not say comes from
// the [wallpaper] section of tiny-shell.ini.

#include <X11/Xlib.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "app/wallpaper.h"
#include "wm/config.h"

int main(int argc, char** argv)
{
    const auto request = tiny::app::parse_wallpaper_args(
            std::vector<std::string>(argv + 1, argv + argc));
    if (!request.error.empty()) {
        fprintf(stderr, "%s: %s\nTry '%s --help'.\n", argv[0],
                request.error.c_str(), argv[0]);
        return 2;
    }
    if (request.help) {
        fputs(tiny::app::wallpaper_usage(argv[0]).c_str(), stdout);
        return EXIT_SUCCESS;
    }

    const tiny::wm::Config config = tiny::wm::load_config(
            tiny::wm::config_files(getenv("XDG_CONFIG_HOME"),
                                   getenv("XDG_CONFIG_DIRS"), getenv("HOME")));
    const std::string file = tiny::app::expand_home(
            request.file.empty() ? config.wallpaper_file : request.file,
            getenv("HOME"));
    if (file.empty()) {
        fprintf(stderr,
                "%s: no wallpaper, give a FILE or set [wallpaper] file\n",
                argv[0]);
        return 1;
    }

    ::Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        fprintf(stderr, "%s: can't open the display\n", argv[0]);
        return 1;
    }
    std::string why;
    const bool done = tiny::app::apply_wallpaper(
            display, XDefaultRootWindow(display), file,
            tiny::app::wallpaper_fit(request.mode.empty()
                                             ? config.wallpaper_mode
                                             : request.mode),
            &why);
    if (done) {
        // the pixmap stays in the server when this program is gone; after a
        // failure nothing is retained, so no empty client is left behind
        XSetCloseDownMode(display, RetainPermanent);
    }
    XCloseDisplay(display);
    if (!done) {
        fprintf(stderr, "%s: %s\n", argv[0], why.c_str());
        return 1;
    }
    return EXIT_SUCCESS;
}
