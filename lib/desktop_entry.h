#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tiny {

/**
 * @short The part of a freedesktop .desktop file that a launcher needs.
 *
 * Only the [Desktop Entry] group is read, other groups (actions) are skipped.
 */
struct DesktopEntry {
    std::string type;
    std::string name;
    std::string generic_name;
    std::string comment;
    std::string exec;
    std::string try_exec;
    std::string icon;
    std::vector<std::string> categories;
    std::vector<std::string> keywords;
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

} // namespace tiny
