#ifndef TINY_HAVE_LUA

#include "plugin/host.h"

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

bool PluginHost::available()
{
    return false;
}

void PluginHost::apply(const std::vector<PluginInfo>& found,
                       const wm::Config& config)
{
    if (impl->told) {
        return;
    }
    for (const PluginInfo& info : found) {
        if (plugin_enabled(info, config)) {
            impl->told = true;
            const std::string line =
                    "tiny-shell: plugins are switched on, but this build has "
                    "no Lua (see LUA in the Makefile)";
            if (impl->services.log) {
                impl->services.log(line);
            } else {
                fprintf(stderr, "%s\n", line.c_str());
            }
            return;
        }
    }
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
