#include "desktop_entry.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <sstream>

namespace tiny {

namespace {

std::string_view trim(std::string_view text)
{
    const char* const blanks = " \t\r\n";
    const auto first = text.find_first_not_of(blanks);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(blanks) - first + 1);
}

// \s \n \t \r \\ of the string values
std::string unescape(std::string_view value)
{
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 == value.size()) {
            out += value[i];
            continue;
        }
        switch (value[++i]) {
            case 's':
                out += ' ';
                break;
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                out += '\r';
                break;
            case '\\':
                out += '\\';
                break;
            default:
                out += '\\';
                out += value[i];
                break;
        }
    }
    return out;
}

// Items separated by ';' (a '\;' is a part of the item), empty ones dropped.
std::vector<std::string> split_list(std::string_view value)
{
    std::vector<std::string> items;
    std::string item;
    const auto flush = [&] {
        if (!item.empty()) {
            items.push_back(unescape(item));
        }
        item.clear();
    };

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == ';') {
            flush();
        } else if (value[i] == '\\' && i + 1 < value.size()) {
            // an escaped backslash must not escape the next character
            if (value[i + 1] == ';') {
                item += ';';
            } else {
                item += value[i];
                item += value[i + 1];
            }
            ++i;
        } else {
            item += value[i];
        }
    }
    flush();
    return items;
}

// "cs_CZ.UTF-8@euro" -> cs_CZ@euro, cs_CZ, cs@euro, cs (the best one first)
std::vector<std::string> locale_candidates(std::string_view locale)
{
    std::string modifier;
    if (const auto at = locale.find('@'); at != std::string_view::npos) {
        modifier = locale.substr(at);
        locale = locale.substr(0, at);
    }
    locale = locale.substr(0, locale.find('.'));

    const std::string full(locale);
    const std::string lang(locale.substr(0, locale.find('_')));
    std::vector<std::string> out;
    if (lang.empty()) {
        return out;
    }
    if (full != lang) {
        if (!modifier.empty()) {
            out.push_back(full + modifier);
        }
        out.push_back(full);
    }
    if (!modifier.empty()) {
        out.push_back(lang + modifier);
    }
    out.push_back(lang);
    return out;
}

// "cs_CZ.UTF-8@euro" -> "cs_CZ@euro", the encoding never takes part
std::string strip_encoding(std::string_view locale)
{
    const auto dot = locale.find('.');
    if (dot == std::string_view::npos) {
        return std::string(locale);
    }
    const auto at = locale.find('@', dot);
    std::string out(locale.substr(0, dot));
    if (at != std::string_view::npos) {
        out += locale.substr(at);
    }
    return out;
}

// only these keys can have a locale postfix
bool is_localized(std::string_view key)
{
    return key == "Name" || key == "GenericName" || key == "Comment" ||
           key == "Keywords" || key == "Icon";
}

} // namespace

bool DesktopEntry::visible() const
{
    return type == "Application" && !name.empty() && !exec.empty() &&
           !no_display && !hidden;
}

DesktopEntry parse_desktop_entry(std::string_view text, std::string_view locale)
{
    const std::vector<std::string> candidates = locale_candidates(locale);

    // the lower the rank, the better; a key without a locale is the worst
    struct Value {
        size_t rank;
        std::string text;
    };
    std::map<std::string, Value, std::less<>> values;

    constexpr std::string_view BOM = "\xEF\xBB\xBF";
    if (text.substr(0, BOM.size()) == BOM) {
        text.remove_prefix(BOM.size());
    }

    bool in_entry = false;
    while (!text.empty()) {
        const auto end = text.find('\n');
        const std::string_view line = trim(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size()
                                                         : end + 1);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line[0] == '[') {
            in_entry = line == "[Desktop Entry]";
            continue;
        }
        const auto equal = line.find('=');
        if (!in_entry || equal == std::string_view::npos) {
            continue;
        }

        std::string_view key = trim(line.substr(0, equal));
        const std::string_view value = trim(line.substr(equal + 1));

        size_t rank = candidates.size();
        if (const auto open = key.find('['); open != std::string_view::npos) {
            if (key.back() != ']') {
                continue;
            }
            const std::string loc =
                    strip_encoding(key.substr(open + 1, key.size() - open - 2));
            key = key.substr(0, open);
            const auto found =
                    std::find(candidates.begin(), candidates.end(), loc);
            if (found == candidates.end() || !is_localized(key)) {
                continue;
            }
            rank = static_cast<size_t>(found - candidates.begin());
        }

        const auto it = values.find(key);
        if (it == values.end()) {
            values.emplace(std::string(key), Value{rank, std::string(value)});
        } else if (rank < it->second.rank) {
            it->second = Value{rank, std::string(value)};
        }
    }

    const auto get = [&](const char* key) -> std::string_view {
        const auto it = values.find(std::string_view(key));
        return it == values.end() ? std::string_view() : it->second.text;
    };

    DesktopEntry entry;
    entry.type = unescape(get("Type"));
    entry.name = unescape(get("Name"));
    entry.generic_name = unescape(get("GenericName"));
    entry.comment = unescape(get("Comment"));
    entry.exec = unescape(get("Exec"));
    entry.try_exec = unescape(get("TryExec"));
    entry.icon = unescape(get("Icon"));
    entry.categories = split_list(get("Categories"));
    entry.keywords = split_list(get("Keywords"));
    entry.terminal = get("Terminal") == "true";
    entry.no_display = get("NoDisplay") == "true";
    entry.hidden = get("Hidden") == "true";
    return entry;
}

std::optional<DesktopEntry> read_desktop_entry(const std::string& path,
                                               std::string_view locale)
{
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    std::string content;
    std::array<char, 4096> buffer;
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        content.append(buffer.data(), static_cast<size_t>(file.gcount()));
    }
    if (file.bad()) {
        return std::nullopt; // a directory, an I/O error
    }
    return parse_desktop_entry(content, locale);
}

std::vector<std::string> split_exec(std::string_view exec)
{
    std::vector<std::string> args;
    std::string arg;
    bool in_arg = false;
    bool quoted = false;

    for (size_t i = 0; i < exec.size(); ++i) {
        const char c = exec[i];
        if (quoted) {
            if (c == '"') {
                quoted = false;
            } else if (c == '\\' && i + 1 < exec.size() &&
                       std::string_view("\"`$\\").find(exec[i + 1]) !=
                               std::string_view::npos) {
                arg += exec[++i];
            } else {
                arg += c;
            }
        } else if (c == '"') {
            quoted = true;
            in_arg = true;
        } else if (c == ' ' || c == '\t') {
            if (in_arg) {
                args.push_back(arg);
                arg.clear();
                in_arg = false;
            }
        } else {
            arg += c;
            in_arg = true;
        }
    }

    if (quoted) {
        return {};
    }
    if (in_arg) {
        args.push_back(arg);
    }
    return args;
}

} // namespace tiny
