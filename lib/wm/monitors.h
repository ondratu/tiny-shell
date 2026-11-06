#pragma once

#include <X11/Xlib.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "widget.h"

namespace tiny::wm {

//! One monitor of the screen, as RandR reports it
struct Monitor {
    std::string name; //!< the output, "HDMI-1"; empty without RandR
    tiny::Rect area = {0, 0, 0, 0}; //!< in the coordinates of the root window
    bool primary = false;           //!< the primary one of RandR
};

/**
 * The monitors in the order that the user sees on the numbers: from the left
 * to the right, the ones with the same left edge from the top to the bottom.
 * The number of a monitor is its index plus one.
 */
using Monitors = std::vector<Monitor>;

/**
 * @short Sort the monitors and drop the ones that show the same picture as an
 * earlier one (a clone is inside of the other monitor), and a monitor that is
 * the whole screen around two other ones.
 */
void order_monitors(Monitors& monitors);

/**
 * @short The monitors of the screen.
 *
 * With RandR (`XRRGetMonitors`) they are the active monitors, ordered by
 * `order_monitors()`. Without RandR (a build without it, a server without the
 * extension) or when it reports none, the whole root window is the one
 * monitor.
 */
Monitors read_monitors(::Display* display, ::Window root);

//! Whether this build has the RandR extension compiled in
bool has_randr();

//! The first event of RandR of the server and whether it has the extension,
//! and ask for the events that tell the monitors changed
std::optional<int> select_monitor_events(::Display* display, ::Window root);

//! Is the event the change of the monitors (`RRScreenChangeNotify`)
bool is_monitor_event(const XEvent& e, int event_base);

//! Tell Xlib about the change that the event reports (it keeps the size of the
//! screen); call it for every event that `is_monitor_event()` accepts
void accept_monitor_event(const XEvent& e);

/**
 * @short The index of the monitor that `spec` names: a number from 1 (the
 * number on the screen) or the name of the output (case does not matter).
 * Nothing when there is none.
 */
std::optional<size_t> find_monitor(const Monitors& monitors,
                                   const std::string& spec);

//! The valid text of a setting of a monitor: "auto", a number or a name
bool valid_monitor_spec(const std::string& text);

//! Which monitors are used by the shell and for the layout
struct MonitorChoice {
    size_t primary = 0;              //!< the panel and the dock are here
    std::optional<size_t> secondary; //!< the other one that is set up
};

/**
 * @short Pick the primary and the secondary monitor from the settings.
 *
 * "auto" (or a text that names no monitor) is the monitor that RandR calls
 * primary, or the first one. The secondary is the first monitor that is not
 * the primary one; nothing when there is just one monitor. The monitors must
 * not be empty.
 */
MonitorChoice choose_monitors(const Monitors& monitors,
                              const std::string& primary_spec,
                              const std::string& secondary_spec);

//! Where the secondary monitor is, seen from the primary one
enum class Placement { Keep, Right, Left, Over, Under };

//! "keep" (or empty), "right", "left", "above" or "below"; nothing otherwise
std::optional<Placement> parse_placement(const std::string& text);

//! The text of a placement as it is written in the file
const char* placement_name(Placement placement);

//! The placements in the order of a form, `Keep` first
const std::vector<Placement>& placements();

//! Is `secondary` already where the placement says, seen from `primary`
bool is_placed(const tiny::Rect& secondary, const tiny::Rect& primary,
               Placement placement);

/**
 * @short The `xrandr` command that puts the secondary monitor where the
 * placement says (`xrandr --output DP-1 --right-of HDMI-1`).
 *
 * Empty when there is nothing to do: `Keep`, a monitor without a name, or the
 * monitors are placed so already.
 */
std::vector<std::string> placement_command(const Monitor& secondary,
                                           const Monitor& primary,
                                           Placement placement);

//! The monitor that has the most of the frame, or the nearest one to it
size_t monitor_of(const Monitors& monitors, const tiny::Rect& frame);

//! The monitor that has the point, the nearest one when it is outside
size_t monitor_at(const Monitors& monitors, int x, int y);

} // namespace tiny::wm
