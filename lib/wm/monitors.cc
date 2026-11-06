#include "wm/monitors.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>

#include "text.h"

#ifdef TINY_RANDR
#include <X11/extensions/Xrandr.h>
#endif

namespace tiny::wm {

namespace {

bool inside(const tiny::Rect& inner, const tiny::Rect& outer)
{
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

bool same_area(const tiny::Rect& a, const tiny::Rect& b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width &&
           a.height == b.height;
}

long overlap(const tiny::Rect& a, const tiny::Rect& b)
{
    const long width =
            std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x);
    const long height =
            std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y);
    return width > 0 && height > 0 ? width * height : 0;
}

//! The square of the distance from the point to the rectangle
long distance2(const tiny::Rect& r, int x, int y)
{
    const long dx = std::max({r.x - x, 0, x - (r.x + r.width - 1)});
    const long dy = std::max({r.y - y, 0, y - (r.y + r.height - 1)});
    return dx * dx + dy * dy;
}

bool same_text(const std::string& a, const std::string& b)
{
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

} // namespace

void order_monitors(Monitors& monitors)
{
    // a monitor around two others is the whole screen, which a server may
    // report beside the monitors that are set up in it; the flags are made
    // first, the vector does not change while they are
    std::vector<bool> whole(monitors.size(), false);
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto inner = std::count_if(
                monitors.begin(), monitors.end(), [&](const Monitor& other) {
                    return &other != &monitors[i] &&
                           inside(other.area, monitors[i].area) &&
                           !same_area(other.area, monitors[i].area);
                });
        whole[i] = inner >= 2;
    }
    size_t at = 0;
    monitors.erase(std::remove_if(monitors.begin(), monitors.end(),
                                  [&](const Monitor&) { return whole[at++]; }),
                   monitors.end());
    std::stable_sort(monitors.begin(), monitors.end(),
                     [](const Monitor& a, const Monitor& b) {
                         return a.area.x != b.area.x ? a.area.x < b.area.x
                                                     : a.area.y < b.area.y;
                     });
    Monitors kept;
    for (Monitor& monitor : monitors) {
        const auto clone = std::find_if(
                kept.begin(), kept.end(), [&monitor](const Monitor& other) {
                    return inside(monitor.area, other.area);
                });
        if (clone != kept.end()) {
            if (monitor.primary) {
                clone->primary =
                        true; // the clone of the primary one is primary
            }
            continue;
        }
        // a bigger monitor that comes later replaces the clones inside of it
        for (auto it = kept.begin(); it != kept.end();) {
            if (inside(it->area, monitor.area)) {
                monitor.primary = monitor.primary || it->primary;
                it = kept.erase(it);
            } else {
                ++it;
            }
        }
        kept.push_back(std::move(monitor));
    }
    monitors = std::move(kept);
}

bool has_randr()
{
#ifdef TINY_RANDR
    return true;
#else
    return false;
#endif
}

namespace {

//! The monitors that RandR reports, ordered; none without RandR
Monitors randr_monitors(::Display* display, ::Window root)
{
    Monitors monitors;
#ifdef TINY_RANDR
    // the monitors are in RandR 1.5; an older server answers with an error.
    // The version does not change while the server runs.
    static const bool has_monitors = [display] {
        int major = 0;
        int minor = 0;
        return XRRQueryVersion(display, &major, &minor) != 0 &&
               major * 100 + minor >= 105;
    }();
    if (!has_monitors) {
        return monitors;
    }
    int count = 0;
    // also the monitors without an output (`xrandr --setmonitor`)
    XRRMonitorInfo* info = XRRGetMonitors(display, root, False, &count);
    if (info != nullptr) {
        for (int i = 0; i < count; ++i) {
            Monitor monitor;
            char* name = XGetAtomName(display, info[i].name);
            if (name != nullptr) {
                monitor.name = name;
                XFree(name);
            }
            monitor.area = {info[i].x, info[i].y, info[i].width,
                            info[i].height};
            monitor.primary = info[i].primary != 0;
            if (monitor.area.width > 0 && monitor.area.height > 0) {
                monitors.push_back(std::move(monitor));
            }
        }
        XRRFreeMonitors(info);
    }
    order_monitors(monitors);
#else
    (void)display;
    (void)root;
#endif
    return monitors;
}

} // namespace

Monitors read_monitors(::Display* display, ::Window root)
{
    Monitors monitors = randr_monitors(display, root);
    if (monitors.empty()) {
        XWindowAttributes attrs = {};
        XGetWindowAttributes(display, root, &attrs);
        monitors.push_back({"", {0, 0, attrs.width, attrs.height}, true});
    }
    return monitors;
}

std::optional<int> select_monitor_events(::Display* display, ::Window root)
{
#ifdef TINY_RANDR
    int event_base = 0;
    int error_base = 0;
    if (XRRQueryExtension(display, &event_base, &error_base)) {
        XRRSelectInput(display, root,
                       RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask |
                               RROutputChangeNotifyMask);
        return event_base;
    }
#else
    (void)display;
    (void)root;
#endif
    return std::nullopt;
}

bool is_monitor_event(const XEvent& e, int event_base)
{
#ifdef TINY_RANDR
    return e.type == event_base + RRScreenChangeNotify ||
           e.type == event_base + RRNotify;
#else
    (void)e;
    (void)event_base;
    return false;
#endif
}

void accept_monitor_event(const XEvent& e)
{
#ifdef TINY_RANDR
    XEvent copy = e; // the function wants a pointer that it may write through
    XRRUpdateConfiguration(&copy);
#else
    (void)e;
#endif
}

std::optional<size_t> find_monitor(const Monitors& monitors,
                                   const std::string& spec)
{
    const std::string text(trim(spec));
    if (text.empty()) {
        return std::nullopt;
    }
    if (std::all_of(text.begin(), text.end(), [](char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        })) {
        if (text.size() > 4) {
            return std::nullopt;
        }
        const unsigned long number = std::strtoul(text.c_str(), nullptr, 10);
        if (number >= 1 && number <= monitors.size()) {
            return number - 1;
        }
        return std::nullopt;
    }
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (same_text(monitors[i].name, text)) {
            return i;
        }
    }
    return std::nullopt;
}

bool valid_monitor_spec(const std::string& text)
{
    const std::string spec(trim(text));
    return std::none_of(spec.begin(), spec.end(), [](char c) {
        return std::iscntrl(static_cast<unsigned char>(c)) != 0 || c == ' ';
    });
}

MonitorChoice choose_monitors(const Monitors& monitors,
                              const std::string& primary_spec,
                              const std::string& secondary_spec)
{
    MonitorChoice choice;
    const auto flagged =
            std::find_if(monitors.begin(), monitors.end(),
                         [](const Monitor& m) { return m.primary; });
    if (const auto found = find_monitor(monitors, primary_spec)) {
        choice.primary = *found;
    } else if (flagged != monitors.end()) {
        choice.primary = static_cast<size_t>(flagged - monitors.begin());
    }
    const auto other = find_monitor(monitors, secondary_spec);
    if (other && *other != choice.primary) {
        choice.secondary = other;
        return choice;
    }
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (i != choice.primary) {
            choice.secondary = i;
            break;
        }
    }
    return choice;
}

std::optional<Placement> parse_placement(const std::string& text)
{
    const std::string value(trim(text));
    for (const Placement placement : placements()) {
        if (same_text(value, placement_name(placement))) {
            return placement;
        }
    }
    if (value.empty()) {
        return Placement::Keep;
    }
    return std::nullopt;
}

const char* placement_name(Placement placement)
{
    switch (placement) {
        case Placement::Keep:
            return "keep";
        case Placement::Right:
            return "right";
        case Placement::Left:
            return "left";
        case Placement::Over:
            return "above";
        case Placement::Under:
            break;
    }
    return "below";
}

const std::vector<Placement>& placements()
{
    static const std::vector<Placement> all = {
            Placement::Keep, Placement::Right, Placement::Left, Placement::Over,
            Placement::Under};
    return all;
}

bool is_placed(const tiny::Rect& secondary, const tiny::Rect& primary,
               Placement placement)
{
    switch (placement) {
        case Placement::Keep:
            return true;
        case Placement::Right:
            return secondary.x == primary.x + primary.width;
        case Placement::Left:
            return secondary.x + secondary.width == primary.x;
        case Placement::Over:
            return secondary.y + secondary.height == primary.y;
        case Placement::Under:
            break;
    }
    return secondary.y == primary.y + primary.height;
}

std::vector<std::string> placement_command(const Monitor& secondary,
                                           const Monitor& primary,
                                           Placement placement)
{
    if (placement == Placement::Keep || secondary.name.empty() ||
        primary.name.empty() || secondary.name == primary.name ||
        is_placed(secondary.area, primary.area, placement)) {
        return {};
    }
    const char* option = "--right-of";
    switch (placement) {
        case Placement::Left:
            option = "--left-of";
            break;
        case Placement::Over:
            option = "--above";
            break;
        case Placement::Under:
            option = "--below";
            break;
        case Placement::Keep:
        case Placement::Right:
            break;
    }
    return {"xrandr", "--output", secondary.name, option, primary.name};
}

size_t monitor_of(const Monitors& monitors, const tiny::Rect& frame)
{
    size_t best = 0;
    long best_overlap = -1;
    for (size_t i = 0; i < monitors.size(); ++i) {
        const long shared = overlap(frame, monitors[i].area);
        if (shared > best_overlap) {
            best = i;
            best_overlap = shared;
        }
    }
    if (best_overlap > 0) {
        return best;
    }
    return monitor_at(monitors, frame.x + frame.width / 2,
                      frame.y + frame.height / 2);
}

size_t monitor_at(const Monitors& monitors, int x, int y)
{
    size_t best = 0;
    long best_distance = LONG_MAX;
    for (size_t i = 0; i < monitors.size(); ++i) {
        const long d = distance2(monitors[i].area, x, y);
        if (d < best_distance) {
            best = i;
            best_distance = d;
        }
    }
    return best;
}

} // namespace tiny::wm
