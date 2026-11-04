#pragma once

#include <X11/Xlib.h>

#include <string>
#include <vector>

#include "wm/config.h"

namespace tiny::wm {

//! A line `name: value` of the X resources; an empty value removes the line
struct ResourceChange {
    std::string name;
    std::string value;

    bool operator==(const ResourceChange& other) const
    {
        return name == other.name && value == other.value;
    }
};

/**
 * @short The text of X resources with the changes made.
 *
 * A line of `current` that has a name of a change is dropped; the changes
 * with a value come at the end. The other lines stay as they are.
 */
std::string merge_resources(const std::string& current,
                            const std::vector<ResourceChange>& changes);

/**
 * @short The resources `Xft.*` for a value of an option of `[xft]`.
 *
 * A value that is not set gives lines to remove (so the programs keep what
 * the system says). Hinting makes two resources: `Xft.hinting` and
 * `Xft.hintstyle`.
 */
std::vector<ResourceChange> xft_resources(XftOption option,
                                          const std::string& value);

/**
 * @short Put the changes into `RESOURCE_MANAGER` of the root window.
 *
 * What `xrdb -merge` does, without `xrdb`: the other resources stay. Xft
 * reads them when a program opens its first font, so a program that runs
 * keeps its look and the ones that start later get the new one. The database
 * of resources of `display` is replaced too (Xlib reads the property only
 * when it opens the connection), so the fonts that the caller opens next
 * follow the changes. False when the property can't be read whole.
 */
bool publish_resources(::Display* display,
                       const std::vector<ResourceChange>& changes);

} // namespace tiny::wm
