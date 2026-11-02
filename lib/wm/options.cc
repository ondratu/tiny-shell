#include "wm/options.h"

#include <cstdio>

#ifndef TINY_SHELL_VERSION
#define TINY_SHELL_VERSION "unknown"
#endif

namespace tiny::wm {

namespace {

std::string number_text(double value)
{
    char text[32];
    snprintf(text, sizeof(text), "%g", value);
    return text;
}

} // namespace

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
           "--replace.\n";
}

std::string info_text(double scale, const std::vector<std::string>& app_dirs)
{
    std::string rv = version_text() +
                     "\n"
                     "A very small window manager and desktop shell for X11,\n"
                     "written directly on top of Xlib (BSD 2-Clause License).\n"
                     "https://github.com/ondratu/tiny-shell\n"
                     "\n"
                     "Scale of the theme (TINY_SCALE_FACTOR): " +
                     number_text(scale) +
                     "\n"
                     "Applications are read from:\n";
    for (const std::string& dir : app_dirs) {
        rv += "  " + dir + "\n";
    }
    return rv;
}

} // namespace tiny::wm
