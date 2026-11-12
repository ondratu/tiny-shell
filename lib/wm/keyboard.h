#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <X11/Xlib.h>

#include "wm/config.h"

namespace tiny::wm {

//! How many layouts the keyboard can have at once (the groups of XKB)
constexpr size_t max_keyboard_layouts = 4;

//! A layout of the keyboard, or a variant of it, as the lists of XKB have it
struct KeyboardLayout {
    std::string id;      //!< `cz`
    std::string variant; //!< `qwerty`; empty for the layout itself
    std::string name;    //!< `Czech (QWERTY)`
};

/**
 * @short The layouts and variants that a rules file of XKB lists
 * (`rules/evdev.lst`: the sections `! layout` and `! variant`).
 *
 * Sorted by the name, so a layout is followed by its variants. A line that
 * does not make sense is skipped.
 */
std::vector<KeyboardLayout> parse_keyboard_layouts(std::string_view text);

/**
 * @short The layouts that the system has.
 *
 * It reads `rules/evdev.lst` (or `base.lst`) in the directory `$XKB_CONFIG_ROOT`
 * names or in one of the usual ones of Linux and the BSDs. Empty when there is
 * no file, then the form has nothing to offer.
 */
std::vector<KeyboardLayout> read_keyboard_layouts(const char* config_root);

/**
 * @short Whether the text can be a list of layouts, variants or options.
 *
 * Empty, or names of letters, digits, `_`, `-` and `:` with commas between
 * them (`cz,us`, `,qwerty`, `grp:alt_shift_toggle,caps:escape`). A name does
 * not start with `-`, so `setxkbmap` can't take it for an option of its own.
 */
bool valid_keyboard_list(const std::string& text);

/**
 * @short The keys that switch the layout and the names a form shows for them.
 *
 * The first one is "" (none), then the usual `grp:` options of XKB.
 */
XftChoices keyboard_switch_choices();

//! Whether the text is "" or an option `grp:...` (the usual ones are in
//! `keyboard_switch_choices()`, the others are kept as they are)
bool valid_keyboard_switch(const std::string& text);

//! The option `grp:...` of the list of options ("" when it has none)
std::string keyboard_switch_of(const std::string& options);

//! The list of options with its `grp:` option replaced by `switch_key`
//! (removed when it is empty); the other options stay
std::string keyboard_options_with_switch(const std::string& options,
                                         const std::string& switch_key);

/**
 * @short The arguments of `setxkbmap` that set the layouts.
 *
 * Empty when there are no `layouts`. The variants and the options are always
 * set (an empty text clears them), so that the keyboard is what the settings
 * say and not what was set before.
 */
std::vector<std::string> keyboard_arguments(const std::string& layouts,
                                            const std::string& variants,
                                            const std::string& options);

/**
 * @short The layouts in the form of the settings: `cz,us` and `,qwerty`.
 *
 * `chosen` are the layouts in the order of switching. The variants are one
 * for each layout; a text without any variant is empty.
 */
void keyboard_lists(const std::vector<KeyboardLayout>& chosen,
                    std::string& layouts, std::string& variants);

/**
 * @short The chosen layouts as the settings name them: the other way round
 * from `keyboard_lists`. An id that is not in `known` is kept (the system may
 * have fewer layouts than the file), with the name of its id.
 */
std::vector<KeyboardLayout>
keyboard_chosen(const std::string& layouts, const std::string& variants,
                const std::vector<KeyboardLayout>& known);

//! The layouts that the keyboard has, as `setxkbmap -query` tells them
struct KeyboardNames {
    std::string layout;
    std::string variant;
    std::string options;
};

/**
 * @short The names in the value of `_XKB_RULES_NAMES`: the strings rules,
 * model, layout, variant and options, each ended by a zero byte.
 *
 * Nothing (empty names) for a value that is too short.
 */
KeyboardNames parse_xkb_rules_names(std::string_view value);

//! The layouts, variants and options that the X server has now (the property
//! `_XKB_RULES_NAMES` of the root window); empty when it has none
KeyboardNames read_keyboard_names(::Display* display);

/**
 * @short The keys of the shell and what they do, for the list of shortcuts.
 */
struct KeyboardShortcut {
    const char* keys;
    const char* action;
};

//! The shortcuts of the shell (the table of doc/usage.md)
std::vector<KeyboardShortcut> keyboard_shortcuts();

} // namespace tiny::wm
