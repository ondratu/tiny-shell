#include "xdg/application_launch.h"

#include <cerrno>
#include <optional>

#include "spawn.h"

namespace tiny::xdg {

namespace {

// Expand the codes inside one argument (%f %u %c %k %%), drop the deprecated
// ones. Nothing is returned for %F %U %i, which are valid only standing alone.
std::optional<std::string> expand_word(const Application& app,
                                       const std::string& arg,
                                       const std::vector<std::string>& files)
{
    std::string out;
    for (size_t i = 0; i < arg.size(); ++i) {
        if (arg[i] != '%' || i + 1 == arg.size()) {
            out += arg[i];
            continue;
        }
        switch (arg[++i]) {
            case '%':
                out += '%';
                break;
            case 'f':
            case 'u':
                if (!files.empty()) {
                    out += files.front();
                }
                break;
            case 'c':
                out += app.entry.name;
                break;
            case 'k':
                out += app.path;
                break;
            case 'F':
            case 'U':
            case 'i':
                return std::nullopt;
            default: // deprecated
                break;
        }
    }
    return out;
}

} // namespace

std::vector<std::string> expand_exec(const Application& app,
                                     const std::vector<std::string>& files)
{
    std::vector<std::string> argv;
    for (const auto& arg : split_exec(app.entry.exec)) {
        if (arg == "%F" || arg == "%U") {
            argv.insert(argv.end(), files.begin(), files.end());
        } else if (arg == "%i") {
            if (!app.entry.icon.empty()) {
                argv.emplace_back("--icon");
                argv.push_back(app.entry.icon);
            }
        } else if (const auto word = expand_word(app, arg, files)) {
            // an argument that is only a code of a missing file vanishes
            if (!word->empty() || arg.find('%') == std::string::npos) {
                argv.push_back(*word);
            }
        }
    }
    return argv;
}

pid_t launch(const Application& app, const std::vector<std::string>& files,
             const std::vector<std::string>& terminal)
{
    auto argv = expand_exec(app, files);
    if (argv.empty()) {
        errno = EINVAL;
        return -1;
    }
    if (app.entry.terminal) {
        argv.insert(argv.begin(), terminal.begin(), terminal.end());
    }
    return spawn(argv, app.entry.path);
}

} // namespace tiny::xdg
