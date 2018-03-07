#include "wm_manager.h"

int main(int argc, char** argv)
{
    try {
        tiny::Display::init();
        wm::Manager manager;
        manager.main_loop();

    } catch (const std::exception &e) {
        fprintf(stderr, "Failed to initialize window manager.");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
