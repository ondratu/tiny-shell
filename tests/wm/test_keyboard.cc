#include "helpers.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "wm/keyboard.h"

namespace fs = std::filesystem;
using namespace tiny::wm;

namespace {

const char* const rules = R"(! model
  pc105           Generic 105-key PC

! layout
  cz              Czech
  us              English (US)
  de              German

! variant
  qwerty          cz: Czech (QWERTY)
  bksl            cz: Czech (with <\|> key)
  intl            us: English (US, intl., with dead keys)
  bad line
  nolayout        Something

! option
  grp                  Switching to another layout
  grp:alt_shift_toggle Alt+Shift
)";

KeyboardLayout item(const char* id, const char* variant, const char* name)
{
    return {id, variant, name};
}

} // namespace

TEST(KeyboardTest, the_layouts_and_the_variants_are_read_sorted_by_name)
{
    const auto found = parse_keyboard_layouts(rules);
    ASSERT_EQ(found.size(), 6U);
    EXPECT_EQ(found[0].name, "Czech");
    EXPECT_EQ(found[0].id, "cz");
    EXPECT_EQ(found[0].variant, "");
    EXPECT_EQ(found[1].name, "Czech (QWERTY)");
    EXPECT_EQ(found[1].variant, "qwerty");
    EXPECT_EQ(found[2].name, "Czech (with <\\|> key)");
    EXPECT_EQ(found[3].name, "English (US)");
    EXPECT_EQ(found[4].variant, "intl");
    EXPECT_EQ(found[4].id, "us");
    EXPECT_EQ(found[5].name, "German");
}

TEST(KeyboardTest,
     a_line_that_makes_no_sense_and_the_other_sections_are_skipped)
{
    EXPECT_TRUE(parse_keyboard_layouts("").empty());
    EXPECT_TRUE(parse_keyboard_layouts("! model\n  pc105  Generic\n").empty());
    // a variant needs `layout:`
    EXPECT_TRUE(parse_keyboard_layouts("! variant\n  qwerty  Czech\n").empty());
    // a layout without a name
    EXPECT_TRUE(parse_keyboard_layouts("! layout\n  cz\n").empty());
}

TEST(KeyboardTest, the_layouts_are_read_from_the_directory_that_is_named)
{
    const fs::path root =
            fs::temp_directory_path() / ("tiny-kbd" + std::to_string(getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "rules");
    std::ofstream(root / "rules" / "base.lst") << rules;
    const auto found = read_keyboard_layouts(root.c_str());
    EXPECT_EQ(found.size(), 6U);
    // evdev.lst is preferred
    std::ofstream(root / "rules" / "evdev.lst") << "! layout\n  fr  French\n";
    const auto evdev = read_keyboard_layouts(root.c_str());
    ASSERT_EQ(evdev.size(), 1U);
    EXPECT_EQ(evdev[0].id, "fr");
    fs::remove_all(root);
}

TEST(KeyboardTest, a_list_is_made_of_names_and_commas)
{
    EXPECT_TRUE(valid_keyboard_list(""));
    EXPECT_TRUE(valid_keyboard_list("cz,us"));
    EXPECT_TRUE(valid_keyboard_list(",qwerty"));
    EXPECT_TRUE(valid_keyboard_list("grp:alt_shift_toggle,caps:escape"));
    EXPECT_FALSE(valid_keyboard_list("-print"));
    EXPECT_FALSE(valid_keyboard_list("cz us"));
    EXPECT_FALSE(valid_keyboard_list("cz\nus"));
    EXPECT_FALSE(valid_keyboard_list("cz;us"));
}

TEST(KeyboardTest, the_switch_is_none_or_a_grp_option)
{
    const auto choices = keyboard_switch_choices();
    ASSERT_GT(choices.size(), 2U);
    EXPECT_STREQ(choices[0].value, "");
    for (const auto& choice : choices) {
        EXPECT_TRUE(valid_keyboard_switch(choice.value)) << choice.value;
    }
    // an option of the file that the form does not offer is kept
    EXPECT_TRUE(valid_keyboard_switch("grp:shift_caps_toggle"));
    EXPECT_FALSE(valid_keyboard_switch("caps:escape"));
    EXPECT_FALSE(valid_keyboard_switch("grp:"));
    EXPECT_FALSE(valid_keyboard_switch("grp:a,caps:b"));
}

TEST(KeyboardTest, the_switch_is_found_in_the_options_and_replaced_there)
{
    EXPECT_EQ(keyboard_switch_of(""), "");
    EXPECT_EQ(keyboard_switch_of("caps:escape"), "");
    EXPECT_EQ(keyboard_switch_of("caps:escape,grp:toggle"), "grp:toggle");
    EXPECT_EQ(keyboard_options_with_switch("caps:escape,grp:toggle",
                                           "grp:alt_shift_toggle"),
              "caps:escape,grp:alt_shift_toggle");
    EXPECT_EQ(keyboard_options_with_switch("caps:escape", "grp:toggle"),
              "caps:escape,grp:toggle");
    EXPECT_EQ(keyboard_options_with_switch("grp:toggle", ""), "");
    EXPECT_EQ(keyboard_options_with_switch("", "grp:toggle"), "grp:toggle");
    EXPECT_EQ(keyboard_options_with_switch("grp:toggle,caps:escape", ""),
              "caps:escape");
}

TEST(KeyboardTest, setxkbmap_gets_the_layouts_and_clears_the_old_options)
{
    EXPECT_TRUE(keyboard_arguments("", "", "").empty());
    EXPECT_EQ(keyboard_arguments("cz,us", ",qwerty", "grp:toggle"),
              (std::vector<std::string>{"setxkbmap", "-layout", "cz,us",
                                        "-variant", ",qwerty", "-option", "",
                                        "-option", "grp:toggle"}));
    EXPECT_EQ(keyboard_arguments("cz", "", ""),
              (std::vector<std::string>{"setxkbmap", "-layout", "cz",
                                        "-variant", "", "-option", ""}));
}

TEST(KeyboardTest, the_chosen_layouts_make_the_two_lists)
{
    std::string layouts;
    std::string variants;
    keyboard_lists({}, layouts, variants);
    EXPECT_EQ(layouts, "");
    EXPECT_EQ(variants, "");
    keyboard_lists({item("cz", "", "Czech"), item("us", "", "English")},
                   layouts, variants);
    EXPECT_EQ(layouts, "cz,us");
    EXPECT_EQ(variants, ""); // no variant at all
    keyboard_lists({item("cz", "", "Czech"), item("us", "intl", "English")},
                   layouts, variants);
    EXPECT_EQ(layouts, "cz,us");
    EXPECT_EQ(variants, ",intl");
    keyboard_lists({item("cz", "qwerty", "Czech")}, layouts, variants);
    EXPECT_EQ(variants, "qwerty");
}

TEST(KeyboardTest, the_lists_make_the_chosen_layouts_back)
{
    const auto known = parse_keyboard_layouts(rules);
    const auto chosen = keyboard_chosen("cz,us", ",intl", known);
    ASSERT_EQ(chosen.size(), 2U);
    EXPECT_EQ(chosen[0].name, "Czech");
    EXPECT_EQ(chosen[1].name, "English (US, intl., with dead keys)");
    // fewer variants than layouts
    EXPECT_EQ(keyboard_chosen("cz,us", "qwerty", known)[0].name,
              "Czech (QWERTY)");
    EXPECT_EQ(keyboard_chosen("cz,us", "qwerty", known)[1].variant, "");
}

TEST(KeyboardTest, a_layout_the_system_does_not_list_is_kept)
{
    const auto chosen = keyboard_chosen("xx,yy", ",v", {});
    ASSERT_EQ(chosen.size(), 2U);
    EXPECT_EQ(chosen[0].id, "xx");
    EXPECT_EQ(chosen[0].name, "xx");
    EXPECT_EQ(chosen[1].name, "yy (v)");
    EXPECT_TRUE(keyboard_chosen("", "", {}).empty());
}

TEST(KeyboardTest, no_more_layouts_than_the_keyboard_has_groups_are_chosen)
{
    const auto chosen = keyboard_chosen("a,b,c,d,e", "", {});
    ASSERT_EQ(chosen.size(), max_keyboard_layouts);
    EXPECT_EQ(chosen.back().id, "d");
}

TEST(KeyboardTest, the_shortcuts_have_keys_and_an_action)
{
    const auto shortcuts = keyboard_shortcuts();
    EXPECT_GE(shortcuts.size(), 8U);
    for (const auto& shortcut : shortcuts) {
        EXPECT_NE(std::string(shortcut.keys), "");
        EXPECT_NE(std::string(shortcut.action), "");
    }
}

TEST(KeyboardTest, the_names_of_the_rules_are_the_third_to_the_fifth_string)
{
    const std::string value("evdev\0pc105\0cz,us\0,qwerty\0grp:toggle\0", 37);
    const KeyboardNames names = parse_xkb_rules_names(value);
    EXPECT_EQ(names.layout, "cz,us");
    EXPECT_EQ(names.variant, ",qwerty");
    EXPECT_EQ(names.options, "grp:toggle");
    // a value that ends early has no names
    const std::string short_value("evdev\0pc105\0cz\0", 16);
    EXPECT_EQ(parse_xkb_rules_names(short_value).layout, "");
    EXPECT_EQ(parse_xkb_rules_names("").layout, "");
}

TEST(KeyboardTest, the_layouts_of_the_server_are_read_from_its_property)
{
    // what the server has depends on the machine: it has a layout, or none
    const KeyboardNames names = read_keyboard_names(tiny::get_display());
    EXPECT_TRUE(valid_keyboard_list(names.layout));
    EXPECT_TRUE(valid_keyboard_list(names.variant));
}

TEST(KeyboardTest, a_list_is_split_at_the_commas_and_the_empty_items_stay)
{
    EXPECT_EQ(split_keeping_empty(""), (std::vector<std::string>{""}));
    EXPECT_EQ(split_keeping_empty("cz,us"),
              (std::vector<std::string>{"cz", "us"}));
    EXPECT_EQ(split_keeping_empty(",intl"),
              (std::vector<std::string>{"", "intl"}));
    EXPECT_EQ(split_keeping_empty("a,"), (std::vector<std::string>{"a", ""}));
}
