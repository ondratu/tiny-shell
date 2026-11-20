#pragma once

// What the tests of the plugins share: the loop of the shell without a
// display, a directory of plugins, and a fixture with a panel and the services
// of the host.

#include "wm_helpers.h"

#include <poll.h>
#include <sys/stat.h>

#include <chrono>

#include "plugin/host.h"
#include "plugin/plugin_info.h"

using namespace std::chrono_literals;
using namespace tiny::plugin;

namespace {

//! The loop of the shell without the display: timers and watched descriptors
class Loop {
  public:
    tiny::Timers timers;
    tiny::FdWatch watches;

    template <class Done>
    bool run_until(Done done, std::chrono::milliseconds limit = 5s)
    {
        const auto end = tiny::Timers::Clock::now() + limit;
        while (!done() && tiny::Timers::Clock::now() < end) {
            std::vector<pollfd> fds;
            std::vector<tiny::FdWatch::Id> ids;
            watches.prepare(fds, ids);
            const int wait = std::min(timers.timeout_ms(), 20);
            poll(fds.data(), tiny::poll_count(fds), wait < 0 ? 20 : wait);
            timers.run_due();
            watches.dispatch(fds, ids, 0);
        }
        return done();
    }
};

//! A directory with the plugins, removed at the end
class PluginTree {
  public:
    PluginTree()
    {
        static int counter = 0;
        path = std::filesystem::temp_directory_path() /
               ("tiny-host-" + std::to_string(getpid()) + "-" +
                std::to_string(counter++));
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~PluginTree() { std::filesystem::remove_all(path); }
    PluginTree(const PluginTree&) = delete;
    PluginTree& operator=(const PluginTree&) = delete;

    void add(const std::string& id, const std::string& lua,
             const std::string& ini = "[plugin]\nenabled = yes\n",
             const std::string& file = "plugin.lua")
    {
        std::filesystem::create_directories(path / id);
        std::ofstream(path / id / "plugin.ini") << ini;
        std::ofstream(path / id / file) << lua;
    }

    std::vector<PluginInfo> find() const
    {
        PluginDirs dirs;
        dirs.user = path.string();
        return find_plugins(dirs);
    }

    std::filesystem::path path;
};

class HostTest : public ::testing::Test {
  protected:
    HostTest()
    {
        panel.realize(root_window(), 0, 0);
        services.panel = &panel;
        services.timers = &loop.timers;
        services.watches = &loop.watches;
        services.version = "9.8";
        services.theme = [this] { return theme; };
        services.log = [this](const std::string& line) {
            lines.push_back(line);
        };
    }

    //! Whether a line of the log has the text
    bool logged(const std::string& text) const
    {
        return std::any_of(lines.begin(), lines.end(),
                           [&](const std::string& line) {
                               return line.find(text) != std::string::npos;
                           });
    }

    size_t panel_children() { return children_of(panel.get_window()).size(); }

    const WidgetFont font;
    const ShellFonts shell_fonts;
    tiny::wm::Panel panel;
    Loop loop;
    HostServices services;
    std::string theme = "dark";
    std::vector<std::string> lines;
    PluginTree tree;
    tiny::wm::Config config;
};

} // namespace
