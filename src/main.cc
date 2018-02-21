#include "window_manager.h"

int main(int argc, char** argv)
{
    auto wm = WindowManager::create();
    if (!wm) {
        fprintf(stderr, "Failed to initialize window manager.");
        return EXIT_FAILURE;
    }

    wm->main_loop();
    return EXIT_SUCCESS;
}
