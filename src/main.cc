#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include "object.h"
#include "spawn.h"
#include "theme.h"
#include "wm/shell_theme.h"
#include "wm/config.h"
#include "wm/manager.h"
#include "wm/options.h"
#include "wm/selection.h"
#include "wm/xresources.h"
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

//! What the options ask to print, for the actions that do not need X; the
//! exit code, or nothing when tiny-shell is to run
std::optional<int> print_and_exit(const tiny::wm::Options& options,
                                  const char* program,
                                  const tiny::wm::Config& config,
                                  const std::vector<std::string>& files)
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
            const std::string with_local = tiny::xdg::data_dirs_with(
                    data_dirs, tiny::xdg::local_share_dir);
            fputs(tiny::wm::info_text(config, files,
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
    return std::nullopt;
}

//! True when nobody else has the redirect of the root window
bool root_is_free(::Display* display, ::Window root)
{
    wm_detected = false;
    XErrorHandler old = XSetErrorHandler(&on_wm_detected);
    XSelectInput(display, root,
                 SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(display, False);
    XSelectInput(display, root, 0); // Reparent do UnmapWindow, do not track
    XSetErrorHandler(old);
    return !wm_detected;
}

} // namespace

int main(int argc, char** argv)
{
    const tiny::wm::Options options = tiny::wm::parse_options(
            std::vector<std::string>(argv + 1, argv + argc));

    // The settings of the files (only --info and running need them); a
    // problem is only reported, the defaults work
    std::vector<std::string> files;
    tiny::wm::Config config;
    if (options.action == tiny::wm::Options::Action::Run ||
        options.action == tiny::wm::Options::Action::Info) {
        files = tiny::wm::config_files(getenv("XDG_CONFIG_HOME"),
                                       getenv("XDG_CONFIG_DIRS"),
                                       getenv("HOME"));
        std::vector<std::string> problems;
        config = tiny::wm::load_config(files, &problems);
        for (const std::string& problem : problems) {
            fprintf(stderr, "%s: %s\n", argv[0], problem.c_str());
        }
    }

    if (const auto rv = print_and_exit(options, argv[0], config, files)) {
        return *rv;
    }

    // The names of the applications are sorted by the language of the user
    setlocale(LC_COLLATE, "");
    // and the clock shows the days and the date the way the user reads them
    setlocale(LC_TIME, "");

    try {
        tiny::Display::init();

        ::Display* display = tiny::get_display();

        // The rendering of text of [xft] goes to the X resources before the
        // first font is opened, so the shell and the programs started from it
        // use it; what is not set is left as the system has it
        const auto resources = tiny::wm::xft_resources(config);
        if (!resources.empty() &&
            !tiny::wm::publish_resources(display, resources)) {
            fprintf(stderr, "%s: can't change RESOURCE_MANAGER\n", argv[0]);
        }

        // The theme needs the display (the fonts are opened here)
        tiny::wm::ShellTheme& theme = tiny::wm::shell_theme;
        theme.set_font_size(config.font_size);
        theme.scale(config.scale);
        theme.set_font(config.font);
        theme.set_font_style(config.font_style);
        theme.set_variant(config.style);
        theme.init();

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

        // The old manager can still hold the root for a moment after it
        // gave up the selection
        const int attempts = options.replace ? 20 : 1;
        bool free_root = root_is_free(display, root);
        for (int i = 1; !free_root && i < attempts; ++i) {
            usleep(100 * 1000);
            free_root = root_is_free(display, root);
        }

        if (!free_root) {
            tiny::error("Detected another window manager on display %s",
                        XDisplayString(display));
            // it does not own the selection, so --replace cannot ask it to go
            throw std::runtime_error(
                    "Detected another window manager that does not support "
                    "--replace, stop it first.");
        }

        tiny::wm::Manager manager(display, root);
        manager.set_selection(&selection);
        manager.set_power_commands(config.reboot_command,
                                   config.poweroff_command);
        manager.set_terminal(config.terminal);
        manager.set_settings_command(
                tiny::sibling_program(argv[0], "tiny-settings"));
        const std::string user_file = tiny::wm::user_config_file(
                getenv("XDG_CONFIG_HOME"), getenv("HOME"));
        manager.set_favorites(
                config.favorites,
                [argv0 = std::string(argv[0]),
                 user_file](const std::vector<std::string>& list) {
                    std::string error;
                    if (user_file.empty()) {
                        error = "neither XDG_CONFIG_HOME nor HOME is set";
                    } else if (tiny::wm::save_favorites(user_file, list,
                                                        &error)) {
                        return;
                    }
                    fprintf(stderr, "%s: the favorites are not saved: %s\n",
                            argv0.c_str(), error.c_str());
                });
        if (!config.wallpaper_file.empty()) {
            manager.set_wallpaper_command(
                    {tiny::sibling_program(argv[0], "tiny-wallpaper")});
        }
        manager.set_clock_format(config.clock_format);
        selection.announce();
        manager.main_loop();
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to initialize window manager: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
