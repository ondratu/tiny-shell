#include "app_search.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <tuple>

#include "desktop_entry.h"

namespace tiny {

namespace {

// Latin-1 letters U+00C0..U+00FF and U+0100..U+017F without accents ('?' keeps
// the character: the signs of multiplication and division)
const char LATIN_1[] = "aaaaaaaceeeeiiiidnooooo?ouuuuyts"
                       "aaaaaaaceeeeiiiidnooooo?ouuuuyty";
const char LATIN_A[] = "aaaaaaccccccccdd"
                       "ddeeeeeeeeeegggg"
                       "gggghhhhiiiiiiii"
                       "iiiijjkkklllllll"
                       "lllnnnnnnnnnoooo"
                       "oooorrrrrrssssss"
                       "ssttttttuuuuuuuu"
                       "uuuuwwyyyzzzzzzs";

// The length of a UTF-8 sequence starting at text[i] (1 for a broken one) and
// its code point.
std::pair<size_t, uint32_t> decode(std::string_view text, size_t i)
{
    const auto lead = static_cast<unsigned char>(text[i]);
    size_t length = 1;
    uint32_t code = lead;
    uint32_t least = 0; // the smallest code point of the length
    if (lead >= 0xC2 && lead < 0xE0) {
        length = 2;
        code = lead & 0x1FU;
        least = 0x80;
    } else if (lead >= 0xE0 && lead < 0xF0) {
        length = 3;
        code = lead & 0x0FU;
        least = 0x800;
    } else if (lead >= 0xF0 && lead < 0xF5) {
        length = 4;
        code = lead & 0x07U;
        least = 0x10000;
    }
    if (length == 1 || i + length > text.size()) {
        return {1, lead};
    }
    for (size_t k = 1; k < length; ++k) {
        const auto next = static_cast<unsigned char>(text[i + k]);
        if ((next & 0xC0U) != 0x80U) {
            return {1, lead};
        }
        code = (code << 6U) | (next & 0x3FU);
    }
    if (code < least || code > 0x10FFFF) { // overlong or too big
        return {1, lead};
    }
    return {length, code};
}

void encode(std::string& out, uint32_t code)
{
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0U | (code >> 6U));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0U | (code >> 12U));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else {
        out += static_cast<char>(0xF0U | (code >> 18U));
        out += static_cast<char>(0x80U | ((code >> 12U) & 0x3FU));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    }
}

char lower_ascii(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool is_word_char(char c)
{
    // bytes of UTF-8 letters belong to a word, the ASCII signs separate
    return static_cast<unsigned char>(c) >= 0x80 || (c >= '0' && c <= '9') ||
           (lower_ascii(c) >= 'a' && c <= 'z');
}

bool is_blank(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

// 0 for a word at a start of a word of the text, 1 for a word elsewhere
std::optional<int> match_kind(const std::string& text, const std::string& word)
{
    std::optional<int> kind;
    for (size_t at = text.find(word); at != std::string::npos;
         at = text.find(word, at + 1)) {
        if (at == 0 || !is_word_char(text[at - 1])) {
            return 0;
        }
        kind = 1;
    }
    return kind;
}

enum Field : int { NAME, GENERIC_NAME, KEYWORDS, EXEC, FIELDS };

std::string program_name(const std::string& exec)
{
    const auto args = split_exec(exec);
    if (args.empty() || args[0][0] == '%') { // a field code is no program
        return {};
    }
    return args[0].substr(args[0].rfind('/') + 1);
}

// The worst of the best matches of the words, lower is better; -1 for no match
int rank_application(const Application& app,
                     const std::vector<std::string>& words)
{
    const std::string texts[] = {fold_text(app.entry.name),
                                 fold_text(app.entry.generic_name),
                                 {},
                                 fold_text(program_name(app.entry.exec))};
    std::vector<std::string> keywords;
    keywords.reserve(app.entry.keywords.size());
    for (const auto& keyword : app.entry.keywords) {
        keywords.push_back(fold_text(keyword));
    }

    int worst = 0;
    for (const auto& word : words) {
        std::optional<int> best;
        const auto consider = [&](const std::string& text, int field) {
            if (const auto kind = match_kind(text, word)) {
                const int rank = *kind * FIELDS + field;
                best = best ? std::min(*best, rank) : rank;
            }
        };
        for (int field = 0; field < FIELDS; ++field) {
            if (field == KEYWORDS) {
                for (const auto& keyword : keywords) {
                    consider(keyword, field);
                }
            } else {
                consider(texts[field], field);
            }
        }
        if (!best) {
            return -1;
        }
        worst = std::max(worst, *best);
    }
    return worst;
}

} // namespace

std::string fold_text(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const auto [length, code] = decode(text, i);
        if (length == 1) {
            out += lower_ascii(text[i]);
        } else if (code >= 0x300 && code < 0x370) {
            // a combining accent
        } else if (code >= 0xC0 && code < 0x100 &&
                   LATIN_1[code - 0xC0] != '?') {
            if (code == 0xC6 || code == 0xE6) {
                out += "ae";
            } else if (code == 0xDF) {
                out += "ss";
            } else {
                out += LATIN_1[code - 0xC0];
            }
        } else if (code == 0x132 || code == 0x133) {
            out += "ij";
        } else if (code == 0x152 || code == 0x153) {
            out += "oe";
        } else if (code >= 0x100 && code < 0x180) {
            out += LATIN_A[code - 0x100];
        } else if (code == 0xA0 || code == 0xB7 ||
                   (code >= 0x2000 && code < 0x2070)) {
            out += ' '; // no-break space, middle dot, dashes and quotes
        } else if (code >= 0x410 && code < 0x430) {
            encode(out, code + 0x20);
        } else if (code >= 0x400 && code < 0x410) {
            encode(out, code + 0x50);
        } else {
            encode(out, code);
        }
        i += length;
    }
    return out;
}

std::vector<size_t> search_applications(const std::vector<Application>& apps,
                                        std::string_view query,
                                        const std::vector<std::string>& recent)
{
    std::vector<std::string> words;
    std::string word;
    for (const char c : fold_text(query) + ' ') {
        if (!is_blank(c)) {
            word += c;
        } else if (!word.empty()) {
            words.push_back(std::move(word));
            word.clear();
        }
    }

    // rank, position among the recent, index
    std::vector<std::tuple<int, size_t, size_t>> found;
    for (size_t i = 0; i < apps.size(); ++i) {
        const int rank = rank_application(apps[i], words);
        if (rank < 0) {
            continue;
        }
        const auto it = std::find(recent.begin(), recent.end(), apps[i].id);
        found.emplace_back(rank, static_cast<size_t>(it - recent.begin()), i);
    }
    std::sort(found.begin(), found.end());

    std::vector<size_t> result;
    result.reserve(found.size());
    for (const auto& item : found) {
        result.push_back(std::get<2>(item));
    }
    return result;
}

} // namespace tiny
