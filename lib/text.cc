#include "text.h"

#include <algorithm>
#include <cstdio>

namespace tiny {

std::string_view trim(std::string_view text)
{
    const char* const blanks = " \t\r\n";
    const auto first = text.find_first_not_of(blanks);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(blanks) - first + 1);
}

std::vector<std::string> split(std::string_view list, char separator,
                               bool trim_items)
{
    std::vector<std::string> items;
    while (!list.empty()) {
        const auto end = list.find(separator);
        std::string_view item = list.substr(0, end);
        if (trim_items) {
            item = trim(item);
        }
        if (!item.empty()) {
            items.emplace_back(item);
        }
        list.remove_prefix(end == std::string_view::npos ? list.size()
                                                         : end + 1);
    }
    return items;
}

std::string lower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return out;
}

std::string number_text(double value)
{
    char text[32];
    snprintf(text, sizeof(text), "%g", value);
    return text;
}

} // namespace tiny
