#ifndef TINY_HAVE_LUA

#include "plugin/host.h"

#include <algorithm>
#include <cstdio>

namespace tiny::plugin {

// The build without Lua: the plugins are found and listed, but nothing runs.

struct PluginHost::Impl {
    HostServices services;
    bool told = false;
};

PluginHost::PluginHost(HostServices services):
    impl(std::make_unique<Impl>(Impl{std::move(services), false}))
{}

PluginHost::~PluginHost() = default;

bool PluginHost::apply(const std::vector<PluginInfo>&, const wm::Config& config,
                       bool)
{
    // the user switched a plugin on (the ones that run by default are not
    // asked about: the shell does not even look for them without Lua)
    const bool wanted = std::any_of(
            config.plugins.begin(), config.plugins.end(), [](const auto& item) {
                return item.second.enabled.value_or(false);
            });
    if (impl->told || !wanted) {
        return false;
    }
    impl->told = true;
    const std::string line = "tiny-shell: plugins are switched on, but this "
                             "build has no Lua (see LUA in the Makefile)";
    if (impl->services.log) {
        impl->services.log(line);
    } else {
        fprintf(stderr, "%s\n", line.c_str());
    }
    return false;
}

void PluginHost::stop() {}

std::vector<std::string> PluginHost::running() const
{
    return {};
}

std::string PluginHost::error_of(const std::string&) const
{
    return {};
}

} // namespace tiny::plugin

#endif // TINY_HAVE_LUA
