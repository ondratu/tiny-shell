#pragma once

#include <string>
#include <vector>

namespace tiny::xdg {

/// One line of the information about a font.
struct InfoRow {
    std::string label;
    std::string value;
    bool heading = false; //!< a title of a group; the value is empty
};

/// The text of a name of an OpenType/TrueType font in UTF-8: UTF-16BE on the
/// platform 3 (Windows) and 0 (Unicode), ASCII on the platform 1 (Macintosh);
/// the other bytes of the Macintosh table are shown as "?". Empty for a
/// platform that is not known.
std::string font_name_text(int platform, const std::string& bytes);

/**
 * @short What a font file says about itself: the creator, the license, the
 * version and the other texts of the `name` table.
 *
 * Only the English texts. The rows have a label ("Designer") and a value; a
 * text the font does not have is left out. A file that can't be read, or has
 * no `name` table (a bitmap font), has no rows.
 */
std::vector<InfoRow> font_file_info(const std::string& file, int face_index);

/**
 * @short Everything that is known about a family, ready to show: the names,
 * the styles, the format, the languages, the files, and the texts of the main
 * face (see `font_file_info`).
 *
 * Empty for a family that is not installed.
 */
std::vector<InfoRow> font_info(const std::string& family);

} // namespace tiny::xdg
