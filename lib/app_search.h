#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "application_index.h"

namespace tiny {

/**
 * @short Make a text comparable: lower case and with no diacritics.
 *
 * UTF-8 in, UTF-8 out. Letters of Latin-1 and Latin Extended-A lose their
 * accents ("Český" -> "cesky"), combining accents are removed, ASCII and
 * Cyrillic letters are lowered, the ligatures OE, IJ, AE and the sharp s
 * become two letters. The no-break space, the middle dot and the general
 * punctuation (dashes, quotes, spaces) become a space. Everything else is
 * copied as it is, so a Cyrillic letter with an accent is not unaccented.
 */
std::string fold_text(std::string_view text);

/**
 * @short Search the applications and sort the found ones.
 *
 * The query is split by blanks and every word must match (case and accents
 * do not matter) `Name`, `GenericName`, one of `Keywords` or the program
 * name of `Exec` (without the directory and the arguments). The order:
 *
 *  1. a word that starts a word of the text (`fox` in "Fire-fox" but not in
 *     "Firefox"), before a word that is anywhere in the text,
 *  2. the field of the match: Name, GenericName, Keywords, Exec,
 *  3. the applications of `recent` (ids, the latest first) before the others,
 *  4. the order of `apps`.
 *
 * With several words the worst match of the words counts. An empty query
 * finds everything, the recent applications first.
 *
 * Returns the indexes into `apps`.
 */
std::vector<size_t>
search_applications(const std::vector<Application>& apps,
                    std::string_view query,
                    const std::vector<std::string>& recent = {});

} // namespace tiny
