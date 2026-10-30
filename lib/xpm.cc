#include "xpm.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace tiny {

namespace {

constexpr int MAX_SIZE = 8192;
constexpr size_t MAX_COLORS = 1 << 18;
constexpr size_t MAX_CPP = 8;

// The C strings of the file, in the order they are written. Comments and
// everything between the strings are skipped.
std::vector<std::string> strings_of(std::string_view text)
{
    std::vector<std::string> out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text.compare(i, 2, "/*") == 0) {
            const auto end = text.find("*/", i + 2);
            if (end == std::string_view::npos) {
                break;
            }
            i = end + 1;
        } else if (text[i] == '"') {
            std::string item;
            for (++i; i < text.size() && text[i] != '"'; ++i) {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    ++i;
                }
                item += text[i];
            }
            out.push_back(std::move(item));
        }
    }
    return out;
}

std::vector<std::string> words_of(const std::string& line)
{
    std::istringstream in(line);
    std::vector<std::string> words;
    for (std::string word; in >> word;) {
        words.push_back(std::move(word));
    }
    return words;
}

constexpr uint32_t TRANSPARENT = 0;

constexpr uint32_t rgb(unsigned r, unsigned g, unsigned b)
{
    return 0xFF000000u | r << 16 | g << 8 | b;
}

std::optional<unsigned> hex_value(std::string_view digits)
{
    unsigned value = 0;
    const auto result = std::from_chars(
            digits.data(), digits.data() + digits.size(), value, 16);
    if (result.ec != std::errc() ||
        result.ptr != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return value;
}

// the color as written in the file, nothing for a color we don't know
std::optional<uint32_t> parse_color(const std::string& value)
{
    std::string name;
    for (const char c : value) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            name += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (name == "none") {
        return TRANSPARENT;
    }

    if (name[0] == '#') {
        const size_t n = (name.size() - 1) / 3; // digits per channel
        if (n < 1 || n > 4 || name.size() != 1 + 3 * n) {
            return std::nullopt;
        }
        unsigned channel[3] = {};
        for (size_t i = 0; i < 3; ++i) {
            const auto v =
                    hex_value(std::string_view(name).substr(1 + i * n, n));
            if (!v) {
                return std::nullopt;
            }
            // to 8 bits
            if (n == 1) {
                channel[i] = *v * 17;
            } else {
                channel[i] = *v >> (4 * (n - 2));
            }
        }
        return rgb(channel[0], channel[1], channel[2]);
    }

    for (const char* prefix : {"gray", "grey"}) {
        const std::string_view p(prefix);
        if (name.compare(0, p.size(), p) == 0 && name.size() > p.size()) {
            unsigned percent = 0;
            const char* const end = name.data() + name.size();
            const auto r =
                    std::from_chars(name.data() + p.size(), end, percent);
            if (r.ec == std::errc() && r.ptr == end && percent <= 100) {
                const unsigned v = (percent * 255 + 50) / 100;
                return rgb(v, v, v);
            }
        }
    }

    static const std::map<std::string, uint32_t> names = {
            {"black", rgb(0, 0, 0)},
            {"white", rgb(255, 255, 255)},
            {"red", rgb(255, 0, 0)},
            {"green", rgb(0, 255, 0)},
            {"blue", rgb(0, 0, 255)},
            {"yellow", rgb(255, 255, 0)},
            {"cyan", rgb(0, 255, 255)},
            {"magenta", rgb(255, 0, 255)},
            {"gray", rgb(190, 190, 190)},
            {"grey", rgb(190, 190, 190)},
            {"darkgray", rgb(169, 169, 169)},
            {"darkgrey", rgb(169, 169, 169)},
            {"lightgray", rgb(211, 211, 211)},
            {"lightgrey", rgb(211, 211, 211)},
            {"orange", rgb(255, 165, 0)},
            {"purple", rgb(160, 32, 240)},
            {"brown", rgb(165, 42, 42)},
            {"pink", rgb(255, 192, 203)},
            {"gold", rgb(255, 215, 0)},
            {"navy", rgb(0, 0, 128)},
            {"darkgreen", rgb(0, 100, 0)},
            {"darkblue", rgb(0, 0, 139)},
            {"darkred", rgb(139, 0, 0)},
    };
    const auto it = names.find(name);
    if (it == names.end()) {
        return std::nullopt;
    }
    return it->second;
}

// The color of a color line (after the symbol): the first usable of the
// visuals c, g, g4, m, s. Unknown colors are transparent.
uint32_t line_color(const std::vector<std::string>& words, size_t first)
{
    static const char* const KEYS[] = {"c", "g", "g4", "m", "s"};
    const auto is_key = [](const std::string& word) {
        return std::find(std::begin(KEYS), std::end(KEYS), word) !=
               std::end(KEYS);
    };

    std::map<std::string, std::string> by_key;
    std::string* current = nullptr;
    for (size_t i = first; i < words.size(); ++i) {
        if (is_key(words[i])) {
            current = &by_key[words[i]];
        } else if (current != nullptr) {
            *current += (current->empty() ? "" : " ") + words[i];
        }
    }
    for (const char* key : KEYS) {
        const auto it = by_key.find(key);
        if (it != by_key.end()) {
            if (const auto color = parse_color(it->second)) {
                return *color;
            }
        }
    }
    return TRANSPARENT;
}

std::optional<int> positive(const std::string& word, int limit)
{
    int value = 0;
    const char* const end = word.data() + word.size();
    const auto r = std::from_chars(word.data(), end, value);
    if (r.ec != std::errc() || r.ptr != end || value < 1 || value > limit) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<Image> parse_xpm(std::string_view text)
{
    if (text.find("XPM") == std::string_view::npos) {
        return std::nullopt;
    }
    const std::vector<std::string> lines = strings_of(text);
    if (lines.empty()) {
        return std::nullopt;
    }

    const auto header = words_of(lines[0]);
    if (header.size() < 4) {
        return std::nullopt;
    }
    const auto width = positive(header[0], MAX_SIZE);
    const auto height = positive(header[1], MAX_SIZE);
    const auto colors = positive(header[2], static_cast<int>(MAX_COLORS));
    const auto cpp = positive(header[3], static_cast<int>(MAX_CPP));
    if (!width || !height || !colors || !cpp) {
        return std::nullopt;
    }
    const auto symbol = static_cast<size_t>(*cpp);
    const auto rows = static_cast<size_t>(*height);
    if (lines.size() < 1 + static_cast<size_t>(*colors) + rows) {
        return std::nullopt;
    }

    std::map<std::string, uint32_t> palette;
    for (size_t i = 0; i < static_cast<size_t>(*colors); ++i) {
        const std::string& line = lines[1 + i];
        if (line.size() < symbol) {
            return std::nullopt;
        }
        palette[line.substr(0, symbol)] =
                line_color(words_of(line.substr(symbol)), 0);
    }

    Image image;
    image.width = *width;
    image.height = *height;
    image.pixels.reserve(static_cast<size_t>(*width) * rows);
    for (size_t y = 0; y < rows; ++y) {
        const std::string& line = lines[1 + static_cast<size_t>(*colors) + y];
        if (line.size() < static_cast<size_t>(*width) * symbol) {
            return std::nullopt;
        }
        for (size_t x = 0; x < static_cast<size_t>(*width); ++x) {
            const auto it = palette.find(line.substr(x * symbol, symbol));
            if (it == palette.end()) {
                return std::nullopt;
            }
            image.pixels.push_back(it->second);
        }
    }
    return image;
}

} // namespace tiny
