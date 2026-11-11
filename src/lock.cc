// tiny-lock: the screen lock.
//
// It covers the screen, holds the keyboard and the pointer and asks for the
// password of the user (the password of the system, there is none of
// tiny-shell). It does not check the password: tiny-lock-auth does, in a
// process of its own. It has no privileges. The rules are in TODO-LOCK.md.
//
//   tiny-lock [--probe]
//
// --probe only checks that the password can be checked. The configuration
// ([lock] in the file of tiny-shell) is read here and handed to
// tiny-lock-auth, which is next to this program.
//
// Exit status: 0 the screen was unlocked, 1 it could not be locked (no way to
// check the password, another program holds the keyboard...) and the screen
// was left alone, which tiny-shell does not try again. A death by a signal or
// another status (an error after the screen was locked ends with 3) is a crash
// of the lock, which tiny-shell starts again.

#include <X11/Xlib.h>

#include <chrono>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <exception>
#include <string>
#include <vector>

#include "app/app_colors.h"
#include "app/lock_auth.h"
#include "app/lock_check.h"
#include "app/lock_screen.h"
#include "display.h"
#include "display_power.h"
#include "spawn.h"
#include "theme.h"
#include "wm/config.h"

namespace {

constexpr int cant_lock = 1;
//! An error after the screen is locked: not 1, which says "do not try again"
constexpr int failed_while_locked = 3;

// An error of X must not end the lock with the status 1 (Xlib's exit()): a
// crash is started again, and a failed request is not worth a crash
int on_x_error(Display*, XErrorEvent* event)
{
    fprintf(stderr, "tiny-lock: X error %d of request %d\n", event->error_code,
            event->request_code);
    return 0;
}

int on_x_io_error(Display*)
{
    _exit(failed_while_locked); // the connection is gone, nothing is left to do
}

void usage(const char* program)
{
    fprintf(stderr,
            "usage: %s [--probe]\nLocks the screen until the password of "
            "the user is typed.\n",
            program);
}

} // namespace

int main(int argc, char** argv)
{
    bool probe = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--probe") == 0) {
            probe = true;
        } else {
            const bool help = strcmp(argv[i], "--help") == 0;
            usage(argv[0]);
            return help ? EXIT_SUCCESS : cant_lock;
        }
    }
    setlocale(LC_CTYPE, "");
    setlocale(LC_TIME, ""); // the names of the days and the months
    signal(SIGPIPE, SIG_IGN);

    std::vector<std::string> problems;
    const tiny::wm::Config config = tiny::wm::load_config(
            tiny::wm::config_files(getenv("XDG_CONFIG_HOME"),
                                   getenv("XDG_CONFIG_DIRS"), getenv("HOME")),
            &problems);
    for (const std::string& problem : problems) {
        fprintf(stderr, "%s: %s\n", argv[0], problem.c_str());
    }

    // what tiny-lock-auth is told (it is next to this program)
    std::vector<std::string> command = {
            tiny::sibling_program(argv[0], "tiny-lock-auth"), "--method",
            config.lock_auth, "--service", config.lock_pam_service};
    if (!config.lock_checker.empty()) {
        command.insert(command.end(), {"--checker", config.lock_checker});
    }
    if (!config.lock_pwcheck.empty()) {
        command.insert(command.end(), {"--pwcheck", config.lock_pwcheck});
    }

    // Fail closed: if the password can't be checked, nothing is locked,
    // because the screen could not be unlocked
    std::vector<std::string> probing = command;
    probing.emplace_back("--probe");
    if (tiny::app::run_to_end(probing) != 0) {
        fprintf(stderr,
                "%s: the screen is not locked, the password can't be checked "
                "(run %s --probe)\n",
                argv[0], probing.front().c_str());
        return cant_lock;
    }
    if (probe) {
        return EXIT_SUCCESS;
    }

    bool locked = false;
    try {
        tiny::Display::init();
        XSetErrorHandler(on_x_error);
        XSetIOErrorHandler(on_x_io_error);
        tiny::theme.set_font_size(config.font_size);
        tiny::theme.scale(config.scale);
        tiny::theme.set_font(config.font);
        tiny::theme.set_font_style(config.font_style);
        tiny::theme.set_variant(tiny::look_variant(
                config.lock_style,
                tiny::app::variant_in_use(tiny::get_display(),
                                          XDefaultScreen(tiny::get_display()),
                                          config.style)));
        tiny::theme.init();
        int status = EXIT_SUCCESS;
        {
            // the date is the one of the panel, without its time
            tiny::app::LockOptions options;
            options.date_format =
                    tiny::app::date_format_of(config.clock_format);
            // an X server with DPMS switches the display off itself (the shell
            // has set the time); without it the lock goes black in that time
            const tiny::DisplayPower power(tiny::get_display());
            if (config.display_off_minutes > 0 && !power.enabled()) {
                options.blank_after =
                        std::chrono::minutes(config.display_off_minutes);
                options.from_last_input = true;
            }
            tiny::app::LockScreen screen(tiny::get_display(), command,
                                         tiny::app::user_full_name(), options);
            if (screen.lock()) {
                locked = true;
                screen.run();
            } else {
                fprintf(stderr,
                        "%s: the screen is not locked, another program holds "
                        "the keyboard or the pointer\n",
                        argv[0]);
                status = cant_lock;
            }
        }
        tiny::theme.release(); // before the display is closed
        return status;
    } catch (const std::exception& e) {
        fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return locked ? failed_while_locked : cant_lock;
    }
}
