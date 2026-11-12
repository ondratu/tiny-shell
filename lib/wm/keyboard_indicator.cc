#include "wm/keyboard_indicator.h"

#include <algorithm>
#include <cctype>

#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>

#include "display.h"
#include "wm/keyboard.h"
#include "wm/shell_theme.h"

namespace tiny::wm {

namespace {

//! The codes that the panel shows are cut to this many letters
constexpr size_t code_letters = 3;

std::string lower_letters(const std::string& name, size_t count)
{
    std::string code;
    for (const char c : name) {
        if (std::isalpha(static_cast<unsigned char>(c)) != 0) {
            code += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
        }
        if (code.size() == count) {
            break;
        }
    }
    return code;
}

} // namespace

std::string KeyboardGroups::code() const
{
    return current < codes.size() ? codes[current] : "";
}

KeyboardGroups make_keyboard_groups(const std::string& layouts,
                                    const std::vector<std::string>& names,
                                    size_t current)
{
    KeyboardGroups groups;
    for (size_t from = 0; from < layouts.size();) {
        size_t end = layouts.find(',', from);
        end = end == std::string::npos ? layouts.size() : end;
        if (end > from) {
            groups.codes.push_back(layouts.substr(from, end - from));
        }
        from = end + 1;
    }
    if (groups.codes.empty()) {
        for (const std::string& name : names) {
            const std::string code = lower_letters(name, 2);
            if (!code.empty()) {
                groups.codes.push_back(code);
            }
        }
    }
    groups.codes.resize(std::min(groups.codes.size(), max_keyboard_layouts));
    groups.names.reserve(groups.codes.size());
    for (size_t i = 0; i < groups.codes.size(); ++i) {
        groups.names.push_back(i < names.size() && !names[i].empty()
                                       ? names[i]
                                       : groups.codes[i]);
    }
    groups.current = current < groups.codes.size() ? current : 0;
    return groups;
}

XkbGroups::XkbGroups(::Display* display_arg): display(display_arg)
{
    int opcode = 0;
    int error = 0;
    int major = XkbMajorVersion;
    int minor = XkbMinorVersion;
    int base = 0;
    if (XkbQueryExtension(display, &opcode, &base, &error, &major, &minor) !=
        False) {
        event_base = base;
    }
}

void XkbGroups::select_events() const
{
    if (!available()) {
        return;
    }
    XkbSelectEvents(display, XkbUseCoreKbd,
                    XkbNewKeyboardNotifyMask | XkbNamesNotifyMask,
                    XkbNewKeyboardNotifyMask | XkbNamesNotifyMask);
    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify,
                          XkbGroupStateMask, XkbGroupStateMask);
}

bool XkbGroups::is_event(const XEvent& e) const
{
    return available() && e.type == event_base;
}

KeyboardGroups XkbGroups::read() const
{
    if (!available()) {
        return {};
    }
    std::vector<std::string> names;
    XkbDescPtr desc = XkbAllocKeyboard();
    if (desc != nullptr) {
        desc->dpy = display;
        if (XkbGetNames(display, XkbGroupNamesMask, desc) == Success &&
            desc->names != nullptr) {
            for (const Atom atom : desc->names->groups) {
                char* name =
                        atom == None ? nullptr : XGetAtomName(display, atom);
                names.emplace_back(name != nullptr ? name : "");
                if (name != nullptr) {
                    XFree(name);
                }
            }
        }
        XkbFreeKeyboard(desc, XkbGroupNamesMask, True);
    }
    XkbStateRec state = {};
    XkbGetState(display, XkbUseCoreKbd, &state);
    return make_keyboard_groups(read_keyboard_names(display).layout, names,
                                state.group);
}

void XkbGroups::lock(size_t group) const
{
    if (available()) {
        XkbLockGroup(display, XkbUseCoreKbd, static_cast<unsigned>(group));
        XFlush(display);
    }
}

namespace {

constexpr const char* check_mark = "\xE2\x9C\x93  "; //!< and the same space
constexpr const char* no_mark = "    ";

//! The width of the text in the font, in pixels
int text_extent(XftFont* font, const std::string& text)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(tiny::get_display(), font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);
    return extents.xOff;
}

//! The width of the menu: the usual one, or more for a long name of a layout
int menu_width_for(const KeyboardGroups& groups)
{
    XftFont* font = shell_theme.panel_widgets.get_font();
    ::Display* display = tiny::get_display();
    int widest = 0;
    for (const std::string& name : groups.names) {
        if (font == nullptr) {
            break; // no font (a test): the usual width
        }
        widest = std::max(widest, text_extent(font, check_mark + name));
    }
    const int wanted = widest + 2 * shell_theme.menu_padding;
    return std::min(std::max(shell_theme.menu_width, wanted),
                    XDisplayWidth(display, XDefaultScreen(display)));
}

} // namespace

KeyboardMenu::KeyboardMenu(const KeyboardGroups& groups):
    tiny::Popover(menu_width_for(groups),
                  shell_theme.popup_item_height *
                          static_cast<int>(std::max<size_t>(1, groups.size())))
{
    name = "wm_keyboardmenu";
    set_style_override(&shell_theme.panel_widgets);
    for (size_t i = 0; i < groups.size(); ++i) {
        // the one in use has a check mark, the others the same space
        const std::string mark = i == groups.current ? check_mark : no_mark;
        items.push_back(std::make_unique<tiny::LabelButton>(
                get_width(), shell_theme.popup_item_height,
                mark + groups.names[i]));
    }
}

KeyboardMenu::~KeyboardMenu() = default;

void KeyboardMenu::realize(::Window parent_arg, int x, int y)
{
    if (is_realized) {
        return;
    }
    tiny::Popover::realize(parent_arg, x, y);
    int top = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        items[i]->set_text_padding(shell_theme.menu_padding);
        items[i]->set_border(0);
        add(items[i].get(), 0, top);
        items[i]->on_click.connect([this, i](const XEvent&) {
            unmap();
            if (on_choose) {
                on_choose(i);
            }
        });
        top += shell_theme.popup_item_height;
    }
}

KeyboardIndicator::KeyboardIndicator():
    tiny::LabelButton(1, shell_theme.panel, "")
{
    name = "wm_keyboard";
    set_border(0);
    // as wide as the widest code, so that nothing in the panel moves when the
    // layout changes
    resize(text_extent(get_style().get_font(), std::string(code_letters, 'm')) +
                   2 * get_padding(),
           shell_theme.panel);
}

KeyboardIndicator::~KeyboardIndicator() = default;

void KeyboardIndicator::set_events(long mask)
{
    on_click.connect([this](const XEvent& e) { open_menu(e); });
    tiny::LabelButton::set_events(mask);
}

void KeyboardIndicator::set_groups(const KeyboardGroups& groups_arg)
{
    const bool same_menu = groups_arg.codes == groups.codes &&
                           groups_arg.names == groups.names &&
                           groups_arg.current == groups.current;
    groups = groups_arg;
    if (!same_menu) {
        menu.reset(); // made again at the next click, with the mark moved
    }
    set_text(groups.code().substr(0, code_letters));
}

void KeyboardIndicator::open_menu(const XEvent& e)
{
    if (groups.size() < 2) {
        return; // one layout: nothing to choose
    }
    if (!menu) {
        menu = std::make_unique<KeyboardMenu>(groups);
        menu->realize(XDefaultRootWindow(display));
        menu->set_events();
        menu->on_choose = [this](size_t group) {
            if (on_choose) {
                on_choose(group);
            }
        };
    }
    if (menu->get_maped()) {
        return;
    }
    // the right edge of the menu is at the right edge of the button, and the
    // menu stays on the screen
    const int right = e.xbutton.x_root - e.xbutton.x + width;
    const int screen = XDisplayWidth(display, XDefaultScreen(display));
    const int left = std::clamp(right - menu->get_width(), 0,
                                std::max(0, screen - menu->get_width()));
    const int top =
            e.xbutton.y_root - e.xbutton.y + height - menu->get_border();
    menu->popup(left + menu->get_width() / 2, top); // popup() centers on x
}

} // namespace tiny::wm
