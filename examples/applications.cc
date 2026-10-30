// Lists the applications of the current user found by tiny::ApplicationIndex.
// Usage: applications [--all]   (--all also prints the file of each entry)

#include <chrono>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "application_index.h"

int main(int argc, char** argv)
{
    std::setlocale(LC_COLLATE, "");
    const bool all = argc > 1 && std::strcmp(argv[1], "--all") == 0;
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

    for (const auto& app : index.applications()) {
        std::cout << app.entry.name << " | icon=" << app.entry.icon
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
              << " ms\n";
    return 0;
}
