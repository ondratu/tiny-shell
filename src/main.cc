#include <chrono>
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
#include "wm/selection.h"
#include "xdg/application_index.h"

namespace {

bool wm_detected = false;

//! How long --replace waits for the old window manager to exit
constexpr std::chrono::seconds replace_timeout{5};

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

        // The manager selection says who is the window manager (ICCCM 2.8)
        tiny::wm::Selection selection(display, root, XDefaultScreen(display));
        switch (selection.acquire(options.replace, replace_timeout)) {
            case tiny::wm::Selection::Result::Acquired:
                break;
            case tiny::wm::Selection::Result::Occupied:
                throw std::runtime_error(
                        "Another window manager runs, use --replace to take "
                        "over from it.");
            case tiny::wm::Selection::Result::Timeout:
                throw std::runtime_error(
                        "The window manager that runs did not exit.");
            case tiny::wm::Selection::Result::Failed:
                throw std::runtime_error("Could not take the manager "
                                         "selection.");
        }

        XErrorHandler old = XSetErrorHandler(&on_wm_detected);
        XSelectInput(display, root,
                     SubstructureRedirectMask | SubstructureNotifyMask);
        XSync(display, False);
        XSelectInput(display, root, 0); // Reparent do UnmapWindow, do not track
        XSetErrorHandler(old);

        if (wm_detected) {
            tiny::error("Detected another window manager on display %s",
                        XDisplayString(display));
            // it does not own the selection, so --replace cannot ask it to go
            throw std::runtime_error(
                    "Detected another window manager that does not support "
                    "--replace, stop it first.");
        }

        tiny::wm::Manager manager(display, root);
        manager.set_selection(&selection);
        selection.announce();
        manager.main_loop();
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to initialize window manager: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
