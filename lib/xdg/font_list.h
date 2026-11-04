#pragma once

#include <fontconfig/fontconfig.h>

#include <cstddef>
#include <string>
#include <vector>

namespace tiny::xdg {

/**
 * @short Family names ready to show: sorted, each one once.
 *
 * The names are compared without regard to the case of ASCII letters (the
 * exact spelling decides between two that differ only in case, so the order
 * is stable). An empty name and a hidden one (it starts with a dot) are
 * dropped.
 */
std::vector<std::string> sort_families(std::vector<std::string> names);

/**
 * @short The font families installed on the system, from fontconfig.
 *
 * The same thing that `fc-list : family` prints, but without starting a
 * process. A font can have more names (a translated one); the first is used,
 * which is the one that matches the font again when it is opened by name.
 * The list is sorted by `sort_families`; it is empty when fontconfig finds
 * no font.
 */
std::vector<std::string> font_families();

/**
 * @short Style names ready to show: each one once, "Regular" (or "Book")
 * first, the others sorted.
 *
 * The names are compared without regard to the case of ASCII letters. An empty
 * name is dropped.
 */
std::vector<std::string> sort_styles(std::vector<std::string> names);

/**
 * @short The styles of a family that the system has ("Regular", "Bold
 * Italic"...), from fontconfig, sorted by `sort_styles`.
 *
 * The family is matched the way fontconfig does it (the case is ignored); a
 * family that is not installed has no styles.
 */
std::vector<std::string> font_styles(const std::string& family);

/**
 * @short The first characters that a font has, as UTF-8.
 *
 * For a font that has no letters of the Latin alphabet (a font of kanji or
 * of icons): its sample is what it does draw. Spaces, control characters and
 * the codes that are not characters are skipped; `skip` characters that do
 * count are left out first, then at most `count` are taken, in the order of
 * their codes. Null `chars` gives an empty string.
 */
std::string font_sample(const FcCharSet* chars, size_t skip, size_t count);

} // namespace tiny::xdg
