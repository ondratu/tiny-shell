#include <clocale>
#include <stdexcept>

#include "object.h"
#include "wm/manager.h"

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

int main(int /*argc*/, char** /*argv*/)
{
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
