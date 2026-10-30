// Lists the applications of the current user found by tiny::ApplicationIndex
// and the icons found by tiny::IconTheme and decoded by tiny::load_image.
// Usage: applications [--all] [--theme NAME] [--size N]
//   --all    also prints the id and the file of each entry
//   --theme  the icon theme (hicolor by default)
//   --size   the wanted size of the icons (48 by default)

#include <chrono>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "application_index.h"
#include "icon_theme.h"
#include "image.h"

int main(int argc, char** argv)
{
    std::setlocale(LC_COLLATE, "");
    bool all = false;
    std::string theme_name = "hicolor";
    int size = 48;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--all") == 0) {
            all = true;
        } else if (std::strcmp(argv[i], "--theme") == 0 && i + 1 < argc) {
            theme_name = argv[++i];
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            size = static_cast<int>(std::strtol(argv[++i], nullptr, 10));
        } else {
            std::cerr << "usage: applications [--all] [--theme NAME] "
                         "[--size N]\n";
            return 1;
        }
    }
    const char* path = std::getenv("PATH");
    const char* lang = std::getenv("LANG");

    const auto dirs = tiny::application_dirs(std::getenv("XDG_DATA_HOME"),
                                             std::getenv("XDG_DATA_DIRS"),
                                             std::getenv("HOME"));
    for (const auto& dir : dirs) {
        std::cout << "dir: " << dir << '\n';
    }

    const auto start = std::chrono::steady_clock::now();
    tiny::ApplicationIndex index;
    index.scan(dirs, lang != nullptr ? lang : "", path != nullptr ? path : "");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();

    const char* const home = std::getenv("HOME");
    const tiny::IconTheme theme(
            tiny::icon_dirs(std::getenv("XDG_DATA_HOME"),
                            std::getenv("XDG_DATA_DIRS"), home),
            tiny::pixmap_dirs(std::getenv("XDG_DATA_HOME"),
                              std::getenv("XDG_DATA_DIRS"), home),
            theme_name);

    size_t without_icon = 0;
    size_t broken = 0;
    for (const auto& app : index.applications()) {
        const std::string icon = theme.find(app.entry.icon, size);
        if (icon.empty()) {
            ++without_icon;
        }
        std::string decoded;
        if (!icon.empty()) {
            const auto image = tiny::load_image(icon, size);
            if (image) {
                decoded = std::to_string(image->width);
                decoded += "x";
                decoded += std::to_string(image->height);
            } else {
                decoded = "cannot be decoded";
                ++broken;
            }
        }
        std::cout << app.entry.name << " | icon=" << app.entry.icon << " -> "
                  << (icon.empty() ? "(not found)" : icon) << ' ' << decoded
                  << " | exec=" << app.entry.exec;
        if (app.entry.terminal) {
            std::cout << " | terminal";
        }
        if (all) {
            std::cout << " | " << app.id << " | " << app.path;
        }
        std::cout << '\n';
    }
    std::cout << index.applications().size() << " applications in " << ms
              << " ms, " << without_icon << " without an icon, " << broken
              << " that cannot be decoded\n";
    return 0;
}
