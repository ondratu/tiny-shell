#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tiny::xdg {

//! An extra way to start an application (`[Desktop Action <id>]`)
struct DesktopAction {
    std::string id;
    std::string name;
    std::string exec;
};

/**
 * @short The part of a freedesktop .desktop file that a launcher needs.
 *
 * The [Desktop Entry] group and the groups of the actions it lists in
 * `Actions` are read, other groups are skipped.
 */
struct DesktopEntry {
    std::string type;
    std::string name;
    std::string generic_name;
    std::string comment;
    std::string exec;
    std::string try_exec;
    std::string path; //!< working directory of the program (Path)
    std::string icon;
    std::string startup_wm_class; //!< WM_CLASS of the windows (StartupWMClass)
    std::vector<std::string> categories;
    std::vector<std::string> keywords;
    //! Desktops that show it (OnlyShowIn), empty for every one
    std::vector<std::string> only_show_in;
    //! Desktops that do not show it (NotShowIn)
    std::vector<std::string> not_show_in;
    //! In the order of `Actions`; an action with no Name or Exec is dropped
    std::vector<DesktopAction> actions;
    bool terminal = false;
    bool startup_notify = false; //!< tells when it has started (StartupNotify)
    bool no_display = false;
    bool hidden = false;
    bool autostart_enabled = true;   //!< `X-GNOME-Autostart-enabled`
    std::string autostart_condition; //!< `AutostartCondition`; empty for none

    /**
     * Whether the desktops `desktops` (the colon separated list of
     * `$XDG_CURRENT_DESKTOP`) show it: `OnlyShowIn` names one of them, or is
     * not there, and `NotShowIn` names none.
     */
    bool shown_in(std::string_view desktops) const;

    /// An application that the user wants to see in a menu or a launcher.
    bool visible() const;
};

/**
 * @short Parse the text of a .desktop file.
 *
 * `locale` is a value like "cs_CZ.UTF-8" (empty for none). Localized keys are
 * chosen in the order of the specification: lang_COUNTRY@MODIFIER,
 * lang_COUNTRY, lang@MODIFIER, lang and then the key without a locale.
 */
DesktopEntry parse_desktop_entry(std::string_view text,
                                 std::string_view locale = "");

/// Read and parse a file. Returns nothing when the file can't be read.
std::optional<DesktopEntry> read_desktop_entry(const std::string& path,
                                               std::string_view locale = "");

/**
 * @short Split the Exec value into arguments (the quoting rules of the
 * specification).
 *
 * Field codes (%f, %u, ...) are left as they are. Returns an empty vector for
 * an empty value or for a quote that is not closed.
 */
std::vector<std::string> split_exec(std::string_view exec);

} // namespace tiny::xdg
