#include <cerrno>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "wm/reload.h"
#include "wm/selection.h"
#include "wm/session_start.h"
#include "wm/xsettings.h"
#include "wm/xresources.h"
#include "xdg/application_index.h"
#include "xdg/autostart.h"
#include "xdg/application_launch.h"
#include "xdg/dirs.h"

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

//! `--reload`: signal the tiny-shell that runs on the display
int reload_running(const char* program)
{
    try {
        tiny::Display::init();
    } catch (const std::exception&) {
        fprintf(stderr, "%s: can't open the X display\n", program);
        return EXIT_FAILURE;
    }
    ::Display* display = tiny::get_display();
    const std::string error =
            tiny::wm::request_reload(display, XDefaultRootWindow(display));
    if (!error.empty()) {
        fprintf(stderr, "%s: %s\n", program, error.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
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
        case Action::Reload:
            return reload_running(program);
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

//! Put the rendering of text of [xft] to the X resources, for the programs
//! that are started after it
void publish_xft(::Display* display, const tiny::wm::Config& config,
                 const char* program)
{
    const auto resources = tiny::wm::xft_resources(config);
    if (!resources.empty() &&
        !tiny::wm::publish_resources(display, resources)) {
        fprintf(stderr, "%s: can't change RESOURCE_MANAGER\n", program);
    }
}

//! Give the manager the settings that can change while it runs. `old` is the
//! settings that it has now, null for a manager that has none yet.
void apply_config(tiny::wm::Manager& manager, const tiny::wm::Config& config,
                  const tiny::wm::Config* old, const char* program)
{
    if (old != nullptr && old->style != config.style) {
        manager.set_variant(config.style);
    }
    if (old != nullptr && (old->panel_style != config.panel_style ||
                           old->dock_style != config.dock_style ||
                           old->launcher_style != config.launcher_style)) {
        manager.set_looks(config.panel_style, config.dock_style,
                          config.launcher_style);
    }
    manager.set_power_commands(config.reboot_command, config.poweroff_command);
    manager.set_terminal(config.terminal);
    manager.set_lock_command(config.lock_command);
    if (old == nullptr || old->display_dpms != config.display_dpms ||
        old->display_off_minutes != config.display_off_minutes) {
        manager.set_dpms(config.display_dpms);
        manager.set_display_off(
                std::chrono::minutes(config.display_off_minutes));
    }
    // the lock counts from the display, so it is set after it
    if (old == nullptr ||
        old->lock_delay_minutes != config.lock_delay_minutes) {
        manager.set_lock_delay(std::chrono::minutes(config.lock_delay_minutes));
    }
    const std::string user_file = tiny::wm::user_config_file(
            getenv("XDG_CONFIG_HOME"), getenv("HOME"));
    manager.set_favorites(
            config.favorites, [argv0 = std::string(program), user_file](
                                      const std::vector<std::string>& list) {
                std::string error;
                if (user_file.empty()) {
                    error = "neither XDG_CONFIG_HOME nor HOME is set";
                } else if (tiny::wm::save_favorites(user_file, list, &error)) {
                    return;
                }
                fprintf(stderr, "%s: the favorites are not saved: %s\n",
                        argv0.c_str(), error.c_str());
            });
    if (old == nullptr || old->wallpaper_file != config.wallpaper_file ||
        old->wallpaper_set != config.wallpaper_set ||
        old->wallpaper_mode != config.wallpaper_mode ||
        old->style != config.style) {
        // without [wallpaper] file the wallpaper is the one of the style
        const bool has_wallpaper =
                !tiny::wm::wallpaper_of(config, config.style,
                                        tiny::xdg::default_data_dirs())
                         .empty();
        manager.set_wallpaper_command(
                has_wallpaper ? std::vector<std::string>{tiny::sibling_program(
                                        program, "tiny-wallpaper")}
                              : std::vector<std::string>{},
                !config.wallpaper_set);
    }
    const bool clock_changed =
            old == nullptr || old->clock_format != config.clock_format;
    if (clock_changed) {
        manager.set_clock_format(config.clock_format);
    }
    if (clock_changed || old->system_tray != config.system_tray) {
        // after the clock: it stays at the end
        manager.set_system_tray(config.system_tray);
    }
    // the programs are told the look; a change of the theme or the fonts
    // shows at once (the scale and the fonts of the shell itself wait for a
    // restart, but the programs can follow)
    manager.set_xsettings(config.xsettings, [config](tiny::Variant variant) {
        return tiny::wm::xsettings_of(config, variant);
    });
    if (old == nullptr || old->dock_mode != config.dock_mode) {
        manager.set_dock_mode(config.dock_mode);
    }
    if (old == nullptr || old->monitor_primary != config.monitor_primary ||
        old->monitor_secondary != config.monitor_secondary ||
        old->monitor_placement != config.monitor_placement) {
        manager.set_monitors(config.monitor_primary, config.monitor_secondary,
                             config.monitor_placement);
    }
}

//! Start the programs of the autostart directories, once in a session (a shell
//! that starts again, `--replace` for one, does not start them again)
void start_autostart(::Display* display, ::Window root,
                     const tiny::wm::Config& config, const char* program)
{
    if (!config.session_autostart ||
        !tiny::wm::claim_autostart(display, root)) {
        return;
    }
    const char* path = getenv("PATH");
    const char* desktops = getenv("XDG_CURRENT_DESKTOP");
    for (const tiny::xdg::Application& app : tiny::xdg::autostart_applications(
                 tiny::xdg::config_dir_list(getenv("XDG_CONFIG_HOME"),
                                            getenv("XDG_CONFIG_DIRS"),
                                            getenv("HOME")),
                 desktops == nullptr ? "" : desktops,
                 path == nullptr ? "" : path)) {
        if (tiny::xdg::launch(app, {}, config.terminal) < 0) {
            fprintf(stderr, "%s: can't start %s (%s): %s\n", program,
                    app.id.c_str(), app.entry.exec.c_str(), strerror(errno));
        }
    }
}

//! SIGHUP and `--reload`: read the files again and apply what changed
void reload_config(tiny::wm::Manager& manager, tiny::wm::Config& current,
                   const char* program)
{
    const std::vector<std::string> files =
            tiny::wm::config_files(getenv("XDG_CONFIG_HOME"),
                                   getenv("XDG_CONFIG_DIRS"), getenv("HOME"));
    std::vector<std::string> problems;
    const tiny::wm::Config config = tiny::wm::load_config(files, &problems);
    for (const std::string& problem : problems) {
        fprintf(stderr, "%s: %s\n", program, problem.c_str());
    }
    if (config.scale != current.scale || config.font != current.font ||
        config.font_size != current.font_size ||
        config.font_style != current.font_style) {
        fprintf(stderr, "%s: the scale and the font are used after a restart\n",
                program);
    }
    if (config.xft_antialias != current.xft_antialias ||
        config.xft_hinting != current.xft_hinting ||
        config.xft_rgba != current.xft_rgba) {
        // an option that is not set any more removes its resources
        std::vector<tiny::wm::ResourceChange> changes =
                tiny::wm::xft_resources(config);
        const std::string* const before[] = {&current.xft_antialias,
                                             &current.xft_hinting,
                                             &current.xft_rgba};
        const std::string* const now[] = {
                &config.xft_antialias, &config.xft_hinting, &config.xft_rgba};
        for (const auto option :
             {tiny::wm::XftOption::Antialias, tiny::wm::XftOption::Hinting,
              tiny::wm::XftOption::Rgba}) {
            const auto i = static_cast<size_t>(option);
            if (now[i]->empty() && !before[i]->empty()) {
                for (auto& change : tiny::wm::xft_resources(option, "")) {
                    changes.push_back(std::move(change));
                }
            }
        }
        if (!changes.empty() &&
            !tiny::wm::publish_resources(tiny::get_display(), changes)) {
            fprintf(stderr, "%s: can't change RESOURCE_MANAGER\n", program);
        }
    }
    apply_config(manager, config, &current, program);
    // what only a restart applies stays as it was, so it is reported again
    const double scale = current.scale;
    const std::string font = current.font;
    const double font_size = current.font_size;
    const std::string font_style = current.font_style;
    current = config;
    current.scale = scale;
    current.font = font;
    current.font_size = font_size;
    current.font_style = font_style;
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

    // The files of autostart name the desktops in OnlyShowIn: a session that
    // did not say who it is (a bare ~/.xinitrc) is tiny-shell
    setenv("XDG_CURRENT_DESKTOP", "tiny-shell", 0);

    try {
        tiny::Display::init();

        ::Display* display = tiny::get_display();

        // The rendering of text of [xft] goes to the X resources before the
        // first font is opened, so the shell and the programs started from it
        // use it; what is not set is left as the system has it
        publish_xft(display, config, argv[0]);

        // The theme needs the display (the fonts are opened here)
        tiny::wm::ShellTheme& theme = tiny::wm::shell_theme;
        theme.set_font_size(config.font_size);
        theme.scale(config.scale);
        theme.set_font(config.font);
        theme.set_font_style(config.font_style);
        theme.set_variant(config.style);
        theme.set_looks(config.panel_style, config.dock_style,
                        config.launcher_style);
        theme.init();
        // the fonts are closed while the display is open, also when it fails
        struct ReleaseFonts {
            ~ReleaseFonts() { tiny::wm::shell_theme.release(); }
        };
        const ReleaseFonts release_fonts;

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
        manager.set_about_command(tiny::sibling_program(argv[0], "tiny-about"));
        manager.set_settings_command(
                tiny::sibling_program(argv[0], "tiny-settings"));
        apply_config(manager, config, nullptr, argv[0]);
        manager.set_reload([&manager, &config, program = argv[0]] {
            reload_config(manager, config, program);
        });
        selection.announce();
        // the panel and its tray are there, so the programs that put an icon
        // into the tray find it
        start_autostart(display, root, config, argv[0]);
        manager.main_loop();
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to initialize window manager: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
