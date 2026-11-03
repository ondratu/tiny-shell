#include "ini.h"

#include <utility>

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

void Ini::parse(std::string_view text, std::vector<std::string>* errors)
{
    const auto fail = [&](size_t number, const char* what) {
        if (errors) {
            errors->push_back(std::to_string(number) + ": " + what);
        }
    };

    constexpr std::string_view bom = "\xEF\xBB\xBF";
    if (text.substr(0, bom.size()) == bom) {
        text.remove_prefix(bom.size());
    }

    std::string section;
    size_t number = 0;
    while (!text.empty()) {
        const auto end = text.find('\n');
        const std::string_view line = trim(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size()
                                                         : end + 1);
        ++number;

        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line[0] == '[') {
            const auto close = line.find(']');
            if (close == std::string_view::npos) {
                fail(number, "the section has no closing ]");
                continue;
            }
            const std::string_view rest = trim(line.substr(close + 1));
            if (!rest.empty() && rest[0] != '#' && rest[0] != ';') {
                fail(number, "unexpected text after the section");
                continue;
            }
            section = trim(line.substr(1, close - 1));
            continue;
        }
        const auto equal = line.find('=');
        if (equal == std::string_view::npos) {
            fail(number, "expected key = value");
            continue;
        }
        const std::string_view key = trim(line.substr(0, equal));
        if (key.empty()) {
            fail(number, "the key is empty");
            continue;
        }
        values[{section, std::string(key)}] =
                std::string(trim(line.substr(equal + 1)));
    }
}

std::optional<std::string> Ini::get(const std::string& section,
                                    const std::string& key) const
{
    const auto it = values.find({section, key});
    if (it == values.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Ini::set(const std::string& section, const std::string& key,
              std::string value)
{
    values[{section, key}] = std::move(value);
}

std::string Ini::dump() const
{
    std::string text;
    const std::string* section = nullptr;
    for (const auto& [name, value] : values) {
        if (section == nullptr || *section != name.first) {
            section = &name.first;
            if (!section->empty()) {
                text += (text.empty() ? "[" : "\n[") + *section + "]\n";
            }
        }
        text += name.second + " = " + value + "\n";
    }
    return text;
}

} // namespace tiny
