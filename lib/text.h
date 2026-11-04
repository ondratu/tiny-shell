#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tiny {

//! `text` without the blanks (space, tab, CR, LF) at both ends
std::string_view trim(std::string_view text);

/**
 * @short The items of a list with `separator` between them.
 *
 * An empty item is dropped; with `trim_items` the blanks around an item are
 * dropped first, so an item of blanks is empty too.
 */
std::vector<std::string> split(std::string_view list, char separator,
                               bool trim_items = false);

//! The letters of ASCII in lower case, the other bytes as they are
std::string lower(std::string_view text);

} // namespace tiny
