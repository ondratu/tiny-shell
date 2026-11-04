#include "wm/action_registry.h"

#include <cerrno>
#include <cstring>

#include "object.h"
#include "spawn.h"

namespace tiny::wm {

void ActionRegistry::add(const std::string& name, Action action)
{
    actions[name] = std::move(action);
}

bool ActionRegistry::run(const std::string& name) const
{
    const auto found = actions.find(name);
    if (found != actions.end()) {
        found->second();
        return true;
    }
    const std::string prefix = exec_prefix;
    if (name.compare(0, prefix.size(), prefix) != 0 ||
        name.size() == prefix.size()) {
        return false;
    }
    const std::string command = name.substr(prefix.size());
    if (tiny::spawn_command(command) < 0) {
        tiny::error("Can't run '%s': %s", command.c_str(), strerror(errno));
    }
    return true;
}

std::vector<std::string> ActionRegistry::names() const
{
    std::vector<std::string> rv;
    rv.reserve(actions.size());
    for (const auto& entry : actions) {
        rv.push_back(entry.first);
    }
    return rv;
}

} // namespace tiny::wm
