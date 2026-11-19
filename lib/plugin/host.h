#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "fd_watch.h"
#include "plugin/plugin_info.h"
#include "timers.h"
#include "wm/config.h"
#include "wm/panel.h"

namespace tiny::plugin {

//! What the plugins get from the shell
struct HostServices {
    wm::Panel* panel = nullptr; //!< where the widgets go; must be realized
    Timers* timers = nullptr;
    FdWatch* watches = nullptr;
    //! The symbolic icons for the widgets; null: the widgets have no icons
    wm::SymbolicIcons* icons = nullptr;
    //! The style of the theme in use: "dark" or "light"
    std::function<std::string()> theme;
    //! A line for the log; empty for the standard error
    std::function<void(const std::string&)> log;
    std::string version; //!< `tiny.version`
};

/**
 * @short Runs the Lua plugins.
 *
 * Every plugin has its own Lua state, so a plugin cannot reach another one,
 * and an error stops only the plugin that made it. The code runs in the loop
 * of the shell and must not block it: a program is started with `tiny.exec`
 * and the result is called later, a function that runs for more than two
 * seconds is stopped.
 *
 * Without Lua (`LUA=no`, or no Lua found by the build) `available()` is false
 * and `apply()` runs nothing. Most of the plugins' interface is described in
 * doc/developers/plugins.md.
 */
class PluginHost {
  public:
    explicit PluginHost(HostServices services);

    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    //! Whether this build can run plugins. A constant, so a build without Lua
    //! leaves out what only the plugins need.
    static constexpr bool available()
    {
#ifdef TINY_HAVE_LUA
        return true;
#else
        return false;
#endif
    }

    /**
     * @short Make the running plugins what the configuration wants.
     *
     * A plugin runs when `plugin_enabled()` says so. When the set of the
     * running plugins changes, or the settings of one that can't follow them
     * live (it did not call `tiny.on_config`), all of them are stopped and
     * started again, so the widgets stay in the order of the ids; otherwise
     * the plugins that asked for it are told that the configuration changed.
     * A plugin that fails to start is logged and left stopped.
     *
     * `restart` starts them again whatever changed: the caller changed the
     * panel and the widgets of the plugins have to be added after it.
     *
     * Returns true when the plugins were started again (the widgets of the
     * plugins are new and at the end of the panel), so the caller can put
     * what must stay after them there again.
     */
    bool apply(const std::vector<PluginInfo>& found, const wm::Config& config,
               bool restart = false);

    //! Stop all the plugins; their widgets are removed from the panel
    void stop();

    //! The ids of the plugins that run, in the order they started
    std::vector<std::string> running() const;

    //! What went wrong in the plugin that did not start, empty when nothing
    std::string error_of(const std::string& id) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace tiny::plugin
