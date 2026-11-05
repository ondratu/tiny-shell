#include "wm/options.h"

#include "text.h"

#ifndef TINY_SHELL_VERSION
#define TINY_SHELL_VERSION "unknown"
#endif

namespace tiny::wm {

Options parse_options(const std::vector<std::string>& args)
{
    Options rv;
    Options::Action asked = Options::Action::Run;

    for (const std::string& arg : args) {
        if (arg == "--replace") {
            rv.replace = true;
        } else if (arg == "--help" || arg == "-h") {
            asked = Options::Action::Help;
        } else if (arg == "--version" || arg == "-V") {
            // help is more than the version, when both are asked
            if (asked != Options::Action::Help) {
                asked = Options::Action::Version;
            }
        } else if (arg == "--info") {
            if (asked == Options::Action::Run) {
                asked = Options::Action::Info;
            }
        } else {
            rv.action = Options::Action::Error;
            rv.error = arg.rfind('-', 0) == 0
                               ? "unknown option '" + arg + "'"
                               : "unexpected argument '" + arg + "'";
            return rv;
        }
    }
    rv.action = asked;
    return rv;
}

std::string version_text()
{
    return "tiny-shell " TINY_SHELL_VERSION;
}

std::string usage_text(const std::string& program)
{
    return "Usage: " + program +
           " [OPTION]...\n"
           "\n"
           "A very small window manager and desktop shell for X11.\n"
           "\n"
           "Options:\n"
           "      --replace  take over from the window manager that is\n"
           "                 running now (it has to support the WM_Sn\n"
           "                 selection of ICCCM); without it tiny-shell\n"
           "                 stops when another window manager runs\n"
           "      --info     describe tiny-shell and its settings\n"
           "  -V, --version  print the version\n"
           "  -h, --help     print this help\n"
           "\n"
           "tiny-shell is stopped by SIGTERM, SIGHUP or SIGINT, and\n"
           "another window manager can take over from it with its own\n"
           "--replace.\n"
           "\n"
           "The settings are read from tiny-shell.ini in $XDG_CONFIG_DIRS\n"
           "(/etc/xdg) and in $XDG_CONFIG_HOME (~/.config), the second one\n"
           "wins; see --info.\n";
}

std::string info_text(const Config& config,
                      const std::vector<std::string>& files,
                      const std::vector<std::string>& app_dirs)
{
    std::string rv = version_text() +
                     "\n"
                     "A very small window manager and desktop shell for X11,\n"
                     "written directly on top of Xlib (BSD 2-Clause License).\n"
                     "https://github.com/ondratu/tiny-shell\n"
                     "\n"
                     "Configuration files (the last one wins):\n";
    for (const std::string& file : files) {
        rv += "  " + file + "\n";
    }
    rv += std::string("Colors of the theme: ") + variant_name(config.style) +
          "\nScale of the theme: " + number_text(config.scale) +
          "\n"
          "Font: " +
          config.font + " " + number_text(config.font_size) +
          (config.font_style.empty() ? "" : " " + config.font_style) +
          "\n"
          "Wallpaper: " +
          (config.wallpaper_file.empty() ? "none" : config.wallpaper_file) +
          "\n"
          "Clock: " +
          (config.clock_format.empty() ? "none" : config.clock_format) +
          "\n"
          "Terminal: " +
          config.terminal_line +
          "\n"
          "Reboot command: " +
          config.reboot_command +
          "\n"
          "Poweroff command: " +
          config.poweroff_command +
          "\n"
          "Applications are read from:\n";
    for (const std::string& dir : app_dirs) {
        rv += "  " + dir + "\n";
    }
    return rv;
}

} // namespace tiny::wm
