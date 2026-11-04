#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "text.h"

namespace tiny {

/**
 * @short The values of an ini file: `[section]` lines and `key = value` lines.
 *
 * A line that starts with `#` or `;` is a comment (a `#` inside a value is a
 * part of the value, so a command can have it). Names of sections and keys are
 * case sensitive. A comment may follow the `]` of a section. A key before the first section belongs to the section "".
 * A second value of a key replaces the first one, which is also how files
 * are layered: parse the global file and then the file of the user.
 */
class Ini {
  public:
    /**
     * @short Add the values of `text`.
     *
     * A line that is not understood is skipped and described in `errors`
     * (`<line number>: <what is wrong>`), when it is given.
     */
    void parse(std::string_view text,
               std::vector<std::string>* errors = nullptr);

    //! The value, or nothing when the file does not have the key
    std::optional<std::string> get(const std::string& section,
                                   const std::string& key) const;

    //! Set a value; the key and the section are made if they are not there
    void set(const std::string& section, const std::string& key,
             std::string value);

    /**
     * @short The text of a file with all the values.
     *
     * Only the values are written (a comment is not stored), the keys before
     * the first section first, then the sections in the order of their
     * names; `parse` of the text gives the same values back.
     */
    std::string dump() const;

    bool empty() const { return values.empty(); }

  private:
    std::map<std::pair<std::string, std::string>, std::string> values;
};

} // namespace tiny
