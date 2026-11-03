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
    //! In the order of `Actions`; an action with no Name or Exec is dropped
    std::vector<DesktopAction> actions;
    bool terminal = false;
    bool no_display = false;
    bool hidden = false;

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
