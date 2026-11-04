#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tiny::wm {

/**
 * @short Named actions of the shell: what a menu item, a key or a plugin can
 * ask for.
 *
 * An action is a function without arguments. The shell registers its own
 * ("logout", "run-dialog", "next-window"...), and the menus, the key bindings
 * and the scripts (later) only know the names. `run()` also understands
 * `exec:<command>` (a shell command line), which needs no registration.
 */
class ActionRegistry {
  public:
    using Action = std::function<void()>;

    //! Register the action; a name that exists gets the new function
    void add(const std::string& name, Action action);

    //! Run the action of the name; false when there is none (and no `exec:`)
    bool run(const std::string& name) const;

    bool has(const std::string& name) const { return actions.count(name) > 0; }

    //! The registered names in alphabetical order
    std::vector<std::string> names() const;

    //! The prefix of the actions that start a command line
    static constexpr const char* exec_prefix = "exec:";

  private:
    std::map<std::string, Action> actions;
};

} // namespace tiny::wm
