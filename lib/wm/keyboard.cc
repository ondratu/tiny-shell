#include "wm/keyboard.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include <X11/Xatom.h>

#include "text.h"

namespace fs = std::filesystem;

namespace tiny::wm {

namespace {

//! The first word of a line and what is after it
bool word_and_rest(std::string_view text, std::string_view& first,
                   std::string_view& tail)
{
    text = trim(text);
    const size_t end = text.find_first_of(" \t");
    if (text.empty() || end == std::string_view::npos) {
        return false;
    }
    first = text.substr(0, end);
    tail = trim(text.substr(end));
    return !tail.empty();
}

//! The items of a comma separated list; the empty ones stay, so that a variant
//! keeps the layout it belongs to
std::vector<std::string> split_all(const std::string& list)
{
    std::vector<std::string> items{""};
    for (const char c : list) {
        if (c == ',') {
            items.emplace_back();
        } else {
            items.back() += c;
        }
    }
    return items;
}

} // namespace

std::vector<KeyboardLayout> parse_keyboard_layouts(std::string_view text)
{
    std::vector<KeyboardLayout> found;
    enum class Part { Other, Layouts, Variants } part = Part::Other;
    std::istringstream lines{std::string(text)};
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line[0] == '!') {
            const std::string_view heading = trim(line.substr(1));
            part = Part::Other;
            if (heading == "layout") {
                part = Part::Layouts;
            } else if (heading == "variant") {
                part = Part::Variants;
            }
            continue;
        }
        std::string_view word;
        std::string_view rest;
        if (part == Part::Other || !word_and_rest(line, word, rest)) {
            continue;
        }
        if (part == Part::Layouts) {
            found.push_back({std::string(word), "", std::string(rest)});
            continue;
        }
        // a variant: `qwerty  cz: Czech (QWERTY)`
        std::string_view layout;
        std::string_view name;
        if (!word_and_rest(rest, layout, name) || layout.back() != ':') {
            continue;
        }
        layout.remove_suffix(1);
        found.push_back(
                {std::string(layout), std::string(word), std::string(name)});
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const KeyboardLayout& a, const KeyboardLayout& b) {
                         return a.name < b.name;
                     });
    return found;
}

std::vector<KeyboardLayout> read_keyboard_layouts(const char* config_root)
{
    std::vector<std::string> roots;
    if (config_root != nullptr && *config_root != '\0') {
        roots.emplace_back(config_root);
    }
    for (const char* usual :
         {"/usr/share/X11/xkb", "/usr/local/share/X11/xkb",
          "/usr/pkg/share/X11/xkb", "/usr/X11R7/lib/X11/xkb",
          "/usr/X11R6/lib/X11/xkb"}) {
        roots.emplace_back(usual);
    }
    for (const std::string& root : roots) {
        for (const char* file : {"rules/evdev.lst", "rules/base.lst"}) {
            const std::ifstream in(fs::path(root) / file);
            if (!in) {
                continue;
            }
            std::ostringstream text;
            text << in.rdbuf();
            auto layouts = parse_keyboard_layouts(text.str());
            if (!layouts.empty()) {
                return layouts;
            }
        }
    }
    return {};
}

bool valid_keyboard_list(const std::string& text)
{
    char before = ',';
    for (const char c : text) {
        const bool name = std::isalnum(static_cast<unsigned char>(c)) != 0 ||
                          c == '_' || c == ':' || c == '-';
        // a name that starts with a dash could be taken for an option
        if ((!name && c != ',') || (c == '-' && before == ',')) {
            return false;
        }
        before = c;
    }
    return true;
}

XftChoices keyboard_switch_choices()
{
    static constexpr XftChoice keys[] = {
            {"", "None"},
            {"grp:caps_toggle", "Caps Lock"},
            {"grp:win_space_toggle", "Win+Space"},
            {"grp:ctrl_shift_toggle", "Ctrl+Shift"},
            {"grp:alt_shift_toggle", "Alt+Shift"},
            {"grp:alt_space_toggle", "Alt+Space"},
            {"grp:ctrl_space_toggle", "Ctrl+Space"},
            {"grp:shifts_toggle", "Both Shifts"},
            {"grp:toggle", "Right Alt"}};
    return {std::begin(keys), std::end(keys)};
}

bool valid_keyboard_switch(const std::string& text)
{
    return text.empty() ||
           (text.rfind("grp:", 0) == 0 && text.size() > 4 &&
            text.find(',') == std::string::npos && valid_keyboard_list(text));
}

std::string keyboard_switch_of(const std::string& options)
{
    for (const std::string& option : split(options, ',')) {
        if (option.rfind("grp:", 0) == 0) {
            return option;
        }
    }
    return "";
}

std::string keyboard_options_with_switch(const std::string& options,
                                         const std::string& switch_key)
{
    std::string result;
    for (const std::string& option : split(options, ',')) {
        if (option.rfind("grp:", 0) != 0) {
            result += (result.empty() ? "" : ",") + option;
        }
    }
    if (!switch_key.empty()) {
        result += (result.empty() ? "" : ",") + switch_key;
    }
    return result;
}

std::vector<std::string> keyboard_arguments(const std::string& layouts,
                                            const std::string& variants,
                                            const std::string& options)
{
    if (layouts.empty()) {
        return {};
    }
    // `-option ""` clears the options of before, the next one sets the new
    std::vector<std::string> arguments = {"setxkbmap", "-layout", layouts,
                                          "-variant",  variants,  "-option",
                                          ""};
    if (!options.empty()) {
        arguments.insert(arguments.end(), {"-option", options});
    }
    return arguments;
}

void keyboard_lists(const std::vector<KeyboardLayout>& chosen,
                    std::string& layouts, std::string& variants)
{
    layouts.clear();
    variants.clear();
    bool any_variant = false;
    for (size_t i = 0; i < chosen.size(); ++i) {
        layouts += (i == 0 ? "" : ",") + chosen[i].id;
        variants += (i == 0 ? "" : ",") + chosen[i].variant;
        any_variant = any_variant || !chosen[i].variant.empty();
    }
    if (!any_variant) {
        variants.clear();
    }
}

std::vector<KeyboardLayout>
keyboard_chosen(const std::string& layouts, const std::string& variants,
                const std::vector<KeyboardLayout>& known)
{
    std::vector<KeyboardLayout> chosen;
    const std::vector<std::string> ids = split_all(layouts);
    const std::vector<std::string> variant_list = split_all(variants);
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i].empty()) {
            continue;
        }
        const std::string& variant =
                i < variant_list.size() ? variant_list[i] : "";
        const auto found = std::find_if(
                known.begin(), known.end(), [&](const KeyboardLayout& k) {
                    return k.id == ids[i] && k.variant == variant;
                });
        chosen.push_back(found != known.end()
                                 ? *found
                                 : KeyboardLayout{ids[i], variant,
                                                  variant.empty()
                                                          ? ids[i]
                                                          : ids[i] + " (" +
                                                                    variant +
                                                                    ")"});
    }
    return chosen;
}

KeyboardNames parse_xkb_rules_names(std::string_view value)
{
    std::vector<std::string> parts;
    size_t from = 0;
    while (parts.size() < 5) {
        const size_t end = value.find('\0', from);
        if (end == std::string_view::npos) {
            break;
        }
        parts.emplace_back(value.substr(from, end - from));
        from = end + 1;
    }
    if (parts.size() < 5) {
        return {};
    }
    return {parts[2], parts[3], parts[4]};
}

KeyboardNames read_keyboard_names(::Display* display)
{
    const Atom property = XInternAtom(display, "_XKB_RULES_NAMES", True);
    if (property == None) {
        return {};
    }
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    KeyboardNames names;
    if (XGetWindowProperty(display, DefaultRootWindow(display), property, 0,
                           256, False, XA_STRING, &type, &format, &count,
                           &after, &data) == Success &&
        data != nullptr) {
        if (type == XA_STRING && format == 8) {
            names = parse_xkb_rules_names(
                    {reinterpret_cast<const char*>(data), count});
        }
        XFree(data);
    }
    return names;
}

std::vector<KeyboardShortcut> keyboard_shortcuts()
{
    return {{"Super", "The launcher with search"},
            {"Alt+F2", "The run dialog"},
            {"Alt+Tab", "The window switcher"},
            {"Alt+Shift+Tab", "The window switcher, backwards"},
            {"Alt+;", "The windows of the current application"},
            {"Alt+Space", "The menu of the window"},
            {"Alt+F4", "Close the window"},
            {"Super+L", "Lock the screen"},
            {"F11", "Fullscreen"},
            {"Alt or Super + left button", "Move the window by any point"}};
}

} // namespace tiny::wm
