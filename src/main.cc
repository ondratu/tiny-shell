#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include "object.h"
#include "theme.h"
#include "wm/manager.h"
#include "wm/options.h"
#include "xdg/application_index.h"

namespace {

bool wm_detected = false;

int on_wm_detected(::Display* /*display*/, XErrorEvent* error)
{
    if (static_cast<int>(error->error_code) == BadAccess) {
        wm_detected = true;
    }
    return 0;
}

} // namespace

//! What the options ask to print, for the actions that do not need X
int print_and_exit(const tiny::wm::Options& options, const char* program)
{
    using Action = tiny::wm::Options::Action;
    switch (options.action) {
        case Action::Help:
            fputs(tiny::wm::usage_text(program).c_str(), stdout);
            return EXIT_SUCCESS;
        case Action::Version:
            puts(tiny::wm::version_text().c_str());
            return EXIT_SUCCESS;
        case Action::Info: {
            const char* data_dirs = getenv("XDG_DATA_DIRS");
            const std::string with_local =
                    tiny::xdg::data_dirs_with(data_dirs, "/usr/local/share");
            fputs(tiny::wm::info_text(tiny::Theme::env_scale_factor(),
                                      tiny::xdg::application_dirs(
                                              getenv("XDG_DATA_HOME"),
                                              with_local.empty()
                                                      ? nullptr
                                                      : with_local.c_str(),
                                              getenv("HOME")))
                          .c_str(),
                  stdout);
            return EXIT_SUCCESS;
        }
        case Action::Error:
            fprintf(stderr, "%s: %s\nTry '%s --help'.\n", program,
                    options.error.c_str(), program);
            return 2;
        case Action::Run:
            break;
    }
    return -1;
}

int main(int argc, char** argv)
{
    const tiny::wm::Options options = tiny::wm::parse_options(
            std::vector<std::string>(argv + 1, argv + argc));
    if (const int rv = print_and_exit(options, argv[0]); rv >= 0) {
        return rv;
    }

    // The names of the applications are sorted by the language of the user
    setlocale(LC_COLLATE, "");

    try {
        tiny::Display::init();

        ::Display* display = tiny::get_display();

        // The theme needs the display (the fonts are opened here)
        tiny::theme.init();

        const ::Window root = XDefaultRootWindow(display);
        XErrorHandler old = XSetErrorHandler(&on_wm_detected);
        XSelectInput(display, root,
                     SubstructureRedirectMask | SubstructureNotifyMask);
        XSync(display, False);
        XSelectInput(display, root, 0); // Reparent do UnmapWindow, do not track
        XSetErrorHandler(old);

        if (wm_detected) {
            tiny::error("Detected another window manager on display %s",
                        XDisplayString(display));
            throw std::runtime_error("Detected another window manager.");
        }

        tiny::wm::Manager manager(display, root);
        manager.main_loop();
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to initialize window manager: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
