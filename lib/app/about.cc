#include "app/about.h"

namespace tiny::app {

std::vector<std::string> about_lines(const std::string& version,
                                     const wm::SystemInfo& info)
{
    std::vector<std::string> lines = {
            "A small window manager and shell for X11", "Version " + version,
            info.name.empty() ? "System unknown" : info.name};
    if (!info.kernel.empty()) {
        lines.push_back(info.kernel + (info.machine.empty()
                                               ? ""
                                               : " (" + info.machine + ")"));
    }
    lines.push_back(about_license);
    lines.push_back(about_project);
    return lines;
}

} // namespace tiny::app
