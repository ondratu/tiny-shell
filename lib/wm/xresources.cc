#include "wm/xresources.h"

#include <X11/Xatom.h>
#include <X11/Xresource.h>

#include <algorithm>

#include "ini.h"

namespace tiny::wm {

std::string merge_resources(const std::string& current,
                            const std::vector<ResourceChange>& changes)
{
    std::string rv;
    std::string_view rest = current;
    while (!rest.empty()) {
        const auto end = rest.find('\n');
        const std::string_view line = rest.substr(0, end);
        rest.remove_prefix(end == std::string_view::npos ? rest.size()
                                                         : end + 1);
        const std::string name(trim(line.substr(0, line.find(':'))));
        const bool replaced = line.find(':') != std::string_view::npos &&
                              std::any_of(changes.begin(), changes.end(),
                                          [&](const ResourceChange& c) {
                                              return c.name == name;
                                          });
        if (!replaced) {
            rv.append(line).push_back('\n');
        }
    }
    for (const ResourceChange& c : changes) {
        if (!c.value.empty()) {
            rv += c.name + ":\t" + c.value + "\n";
        }
    }
    return rv;
}

std::vector<ResourceChange> xft_resources(XftOption option,
                                          const std::string& value)
{
    switch (option) {
        case XftOption::Antialias: {
            std::string number; // not set removes the resource
            if (value == "yes" || value == "no") {
                number = value == "yes" ? "1" : "0";
            }
            return {{"Xft.antialias", number}};
        }
        case XftOption::Hinting: {
            std::string on; // not set removes both resources
            std::string style;
            if (!value.empty()) {
                on = value == "none" ? "0" : "1";
                style = "hint" + value;
            }
            return {{"Xft.hinting", on}, {"Xft.hintstyle", style}};
        }
        case XftOption::Rgba:
            break;
    }
    return {{"Xft.rgba", value}};
}

std::vector<ResourceChange> xft_resources(const Config& config)
{
    std::vector<ResourceChange> rv;
    const std::string* values[] = {&config.xft_antialias, &config.xft_hinting,
                                   &config.xft_rgba};
    for (const XftOption option :
         {XftOption::Antialias, XftOption::Hinting, XftOption::Rgba}) {
        const std::string& value = *values[static_cast<size_t>(option)];
        if (!value.empty()) {
            for (auto& change : xft_resources(option, value)) {
                rv.push_back(std::move(change));
            }
        }
    }
    return rv;
}

bool publish_resources(::Display* display,
                       const std::vector<ResourceChange>& changes)
{
    // xrdb and the display keep the resources on the first screen
    const ::Window root = RootWindow(display, 0);
    std::string current;
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long left = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display, root, XA_RESOURCE_MANAGER, 0, 1L << 20,
                           False, XA_STRING, &type, &format, &count, &left,
                           &data) != Success) {
        return false; // not read, so what is there is not known
    }
    if (data != nullptr) {
        if (type == XA_STRING && format == 8) {
            current.assign(reinterpret_cast<const char*>(data), count);
        }
        XFree(data);
    }
    if (left != 0) {
        return false; // too long to keep whole, better not to cut it
    }
    const std::string merged = merge_resources(current, changes);
    XChangeProperty(display, root, XA_RESOURCE_MANAGER, XA_STRING, 8,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(merged.data()),
                    static_cast<int>(merged.size()));
    XSync(display, False);
    // Xlib read the resources when the connection was opened and Xft asks
    // that copy, so give the program its own connection the new text too
    XrmInitialize();
    XrmDatabase old = XrmGetDatabase(display);
    XrmSetDatabase(display, XrmGetStringDatabase(merged.c_str()));
    if (old != nullptr) {
        XrmDestroyDatabase(old); // XrmSetDatabase does not free it
    }
    return true;
}

} // namespace tiny::wm
