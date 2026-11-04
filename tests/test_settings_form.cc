#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "settings_form.h"

using tiny::Rect;
using tiny::SettingsForm;
using Field = SettingsForm::Field;
using Key = SettingsForm::Key;
using Result = SettingsForm::Result;
using View = SettingsForm::View;
using Strings = std::vector<std::string>;

namespace {

const Strings families = {"Alpha",   "Beta Mono", "Gamma", "Delta Mono",
                          "Epsilon", "Zeta",      "Eta",   "Theta",
                          "Iota",    "Kappa"};

SettingsForm make(const SettingsForm::Values& values = {"Gamma", "xterm -e",
                                                        "1"},
                  const Strings& list = families)
{
    SettingsForm form(values, list, SettingsForm::Metrics());
    form.resize(500, 300);
    return form;
}

int center_x(const Rect& r)
{
    return r.x + r.width / 2;
}

int center_y(const Rect& r)
{
    return r.y + r.height / 2;
}

//! A click in the middle of a rectangle
bool click(SettingsForm& form, const Rect& r)
{
    form.press(center_x(r), center_y(r));
    return form.release(center_x(r), center_y(r));
}

} // namespace

TEST(SettingsFormTest, starts_with_the_values_and_the_font_field_focused)
{
    const SettingsForm form = make();

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.result(), Result::Editing);
    EXPECT_EQ(form.focus(), Field::Font);
    EXPECT_EQ(form.text(Field::Font), "Gamma");
    EXPECT_EQ(form.text(Field::Terminal), "xterm -e");
    EXPECT_EQ(form.text(Field::Scale), "1");
    EXPECT_EQ(form.values().font, "Gamma");
    EXPECT_TRUE(form.problem().empty());
}

TEST(SettingsFormTest, metrics_follow_the_scale)
{
    const auto one = SettingsForm::Metrics::scaled(1);
    const auto two = SettingsForm::Metrics::scaled(2);

    EXPECT_EQ(two.margin, 2 * one.margin);
    EXPECT_EQ(two.row_height, 2 * one.row_height);
    EXPECT_EQ(two.text_size, 2 * one.text_size);
    EXPECT_EQ(two.choose_width, 2 * one.choose_width);
    EXPECT_EQ(two.label_width, 2 * one.label_width);
    EXPECT_EQ(two.button_width, 2 * one.button_width);
    EXPECT_EQ(two.row_gap, 2 * one.row_gap);
}

TEST(SettingsFormTest, the_form_has_the_fields_one_under_another)
{
    const SettingsForm form = make();

    const Rect font = form.field_rect(Field::Font);
    const Rect terminal = form.field_rect(Field::Terminal);
    const Rect scale = form.field_rect(Field::Scale);
    EXPECT_LT(font.y + font.height, terminal.y);
    EXPECT_LT(terminal.y + terminal.height, scale.y);
    EXPECT_EQ(form.label_rect(Field::Terminal).y, terminal.y);
    // the font field leaves room for the button that opens the list
    EXPECT_LT(font.x + font.width, form.choose_rect().x);
    EXPECT_EQ(terminal.x + terminal.width,
              form.choose_rect().x + form.choose_rect().width);
    // the buttons are at the bottom, Cancel left of Save
    EXPECT_LT(form.cancel_rect().x, form.save_rect().x);
    EXPECT_GT(form.save_rect().y, scale.y);
    EXPECT_LE(form.save_rect().y + form.save_rect().height, 300);
    EXPECT_GT(form.message_rect().y, scale.y);
    EXPECT_LT(form.message_rect().y + form.message_rect().height,
              form.save_rect().y + form.save_rect().height);
}

TEST(SettingsFormTest, the_form_height_shows_everything)
{
    SettingsForm form = make();
    form.resize(500, form.form_height());

    EXPECT_GE(form.save_rect().y,
              form.message_rect().y + form.message_rect().height);
}

TEST(SettingsFormTest, a_tiny_window_does_not_make_negative_sizes)
{
    SettingsForm form = make();
    form.resize(0, 0);

    EXPECT_GE(form.field_rect(Field::Font).width, 1);
    EXPECT_GE(form.field_rect(Field::Terminal).width, 1);
    EXPECT_GE(form.message_rect().width, 1);
    EXPECT_GE(form.search_rect().width, 1);
    EXPECT_GE(form.visible_rows(), 1u);
}

TEST(SettingsFormTest, typing_goes_to_the_focused_field)
{
    SettingsForm form = make();

    EXPECT_TRUE(form.type("X"));
    EXPECT_EQ(form.text(Field::Font), "GammaX");
    EXPECT_TRUE(form.key(Key::Tab));
    EXPECT_TRUE(form.type(" -x"));
    EXPECT_EQ(form.text(Field::Terminal), "xterm -e -x");
    EXPECT_TRUE(form.key(Key::Tab));
    EXPECT_TRUE(form.type("5"));
    EXPECT_EQ(form.text(Field::Scale), "15");
}

TEST(SettingsFormTest, control_characters_are_not_typed)
{
    SettingsForm form = make();

    EXPECT_FALSE(form.type(""));
    EXPECT_FALSE(form.type("\r"));
    EXPECT_FALSE(form.type("\t\x1b"));
    EXPECT_FALSE(form.type("\x7f"));
    EXPECT_TRUE(form.type("a\nb"));
    EXPECT_EQ(form.text(Field::Font), "Gammaab");
}

TEST(SettingsFormTest, backspace_deletes_a_whole_utf8_character)
{
    SettingsForm form = make({"", "", ""});

    EXPECT_FALSE(form.key(Key::Back)); // nothing to delete
    form.type("a\xC5\x99");            // a, r with a caron
    EXPECT_TRUE(form.key(Key::Back));
    EXPECT_EQ(form.text(Field::Font), "a");
    EXPECT_TRUE(form.key(Key::Back));
    EXPECT_TRUE(form.text(Field::Font).empty());
}

TEST(SettingsFormTest, tab_goes_round_the_fields_and_back_tab_the_other_way)
{
    SettingsForm form = make();

    form.key(Key::Tab);
    EXPECT_EQ(form.focus(), Field::Terminal);
    form.key(Key::Tab);
    EXPECT_EQ(form.focus(), Field::Scale);
    form.key(Key::Tab);
    EXPECT_EQ(form.focus(), Field::Font);
    form.key(Key::BackTab);
    EXPECT_EQ(form.focus(), Field::Scale);
    form.key(Key::BackTab);
    EXPECT_EQ(form.focus(), Field::Terminal);
}

TEST(SettingsFormTest, the_arrows_move_between_the_fields)
{
    SettingsForm form = make();

    form.key(Key::Up); // round to the last
    EXPECT_EQ(form.focus(), Field::Scale);
    form.key(Key::Down); // round to the first
    EXPECT_EQ(form.focus(), Field::Font);
    EXPECT_EQ(form.view(), View::Form);
    form.key(Key::Tab);
    form.key(Key::Down);
    EXPECT_EQ(form.focus(), Field::Scale);
    form.key(Key::Up);
    EXPECT_EQ(form.focus(), Field::Terminal);
}

TEST(SettingsFormTest, the_other_keys_do_nothing_in_the_form)
{
    SettingsForm form = make();

    for (const Key key : {Key::PageUp, Key::PageDown, Key::Home, Key::End}) {
        EXPECT_FALSE(form.key(key));
    }
}

TEST(SettingsFormTest, a_click_into_a_field_focuses_it)
{
    SettingsForm form = make();

    EXPECT_TRUE(click(form, form.field_rect(Field::Scale)));
    EXPECT_EQ(form.focus(), Field::Scale);
    EXPECT_TRUE(click(form, form.field_rect(Field::Terminal)));
    EXPECT_EQ(form.focus(), Field::Terminal);
    EXPECT_TRUE(click(form, form.field_rect(Field::Font)));
    EXPECT_EQ(form.focus(), Field::Font);
}

TEST(SettingsFormTest, a_press_that_moves_away_before_the_release_does_nothing)
{
    SettingsForm form = make();

    const Rect scale = form.field_rect(Field::Scale);
    form.press(center_x(scale), center_y(scale));
    const Rect terminal = form.field_rect(Field::Terminal);
    EXPECT_FALSE(form.release(center_x(terminal), center_y(terminal)));
    EXPECT_EQ(form.focus(), Field::Font);
    // a release without a press
    EXPECT_FALSE(form.release(center_x(scale), center_y(scale)));
    // a click into nothing
    form.press(0, 0);
    EXPECT_FALSE(form.release(0, 0));
}

TEST(SettingsFormTest, the_problems_of_the_fields_are_told)
{
    SettingsForm form = make({"", "", "big"});

    EXPECT_FALSE(form.problem(Field::Font).empty());
    EXPECT_FALSE(form.problem(Field::Terminal).empty());
    EXPECT_FALSE(form.problem(Field::Scale).empty());
    EXPECT_EQ(form.problem(), form.problem(Field::Font));

    form.type("Gamma");
    EXPECT_EQ(form.problem(), form.problem(Field::Terminal));
    form.key(Key::Tab);
    form.type("xterm -e");
    EXPECT_EQ(form.problem(), form.problem(Field::Scale));
    form.key(Key::Tab);
    form.key(Key::Tab); // scale again... "big"
    EXPECT_FALSE(form.problem().empty());
}

TEST(SettingsFormTest, a_blank_terminal_is_a_problem)
{
    const SettingsForm form = make({"Gamma", "   ", "1"});

    EXPECT_FALSE(form.problem(Field::Terminal).empty());
}

TEST(SettingsFormTest, the_values_to_write_have_no_blanks_at_the_ends)
{
    const SettingsForm form = make({" Gamma ", " foot -e", "2 "});

    const auto values = form.clean_values();
    EXPECT_EQ(values.font, "Gamma");
    EXPECT_EQ(values.terminal, "foot -e");
    EXPECT_EQ(values.scale, "2");
}

TEST(SettingsFormTest, save_asks_to_write_when_all_is_right)
{
    SettingsForm form = make();

    EXPECT_TRUE(click(form, form.save_rect()));
    EXPECT_EQ(form.result(), Result::Saved);
}

TEST(SettingsFormTest, return_saves_from_any_field)
{
    SettingsForm form = make();
    form.key(Key::Tab);

    EXPECT_TRUE(form.key(Key::Return));
    EXPECT_EQ(form.result(), Result::Saved);
}

TEST(SettingsFormTest, nothing_is_saved_while_something_is_wrong)
{
    SettingsForm form = make({"Gamma", "xterm -e", "0"});

    EXPECT_FALSE(click(form, form.save_rect()));
    EXPECT_FALSE(form.key(Key::Return));
    EXPECT_EQ(form.result(), Result::Editing);
}

TEST(SettingsFormTest, a_failed_write_is_shown_and_editing_goes_on)
{
    SettingsForm form = make();
    form.key(Key::Return);
    ASSERT_EQ(form.result(), Result::Saved);

    form.reject("Not saved: disk is full");

    EXPECT_EQ(form.result(), Result::Editing);
    EXPECT_EQ(form.status(), "Not saved: disk is full");
    // the next edit takes the message away
    form.type("x");
    EXPECT_TRUE(form.status().empty());
    form.reject("again");
    form.key(Key::Back);
    EXPECT_TRUE(form.status().empty());
}

TEST(SettingsFormTest, escape_and_cancel_give_up)
{
    SettingsForm escape = make();
    EXPECT_TRUE(escape.key(Key::Escape));
    EXPECT_EQ(escape.result(), Result::Cancelled);

    SettingsForm cancel = make();
    EXPECT_TRUE(click(cancel, cancel.cancel_rect()));
    EXPECT_EQ(cancel.result(), Result::Cancelled);
}

TEST(SettingsFormTest, the_wheel_does_nothing_in_the_form)
{
    SettingsForm form = make();

    EXPECT_FALSE(form.wheel(1));
}

// The list of fonts

TEST(SettingsFormTest, the_choose_button_opens_the_list_at_the_current_font)
{
    SettingsForm form = make({"gamma", "xterm -e", "1"}); // another case

    EXPECT_TRUE(click(form, form.choose_rect()));

    EXPECT_EQ(form.view(), View::Picker);
    EXPECT_EQ(form.families().size(), families.size());
    EXPECT_EQ(form.families()[form.selected()], "Gamma");
    EXPECT_TRUE(form.query().empty());
}

TEST(SettingsFormTest, the_list_starts_at_the_top_for_a_font_that_is_not_there)
{
    SettingsForm form = make({"Nothing", "xterm -e", "1"});

    form.open_picker();

    EXPECT_EQ(form.selected(), 0u);
}

TEST(SettingsFormTest, down_in_the_font_field_opens_the_list)
{
    SettingsForm form = make();

    EXPECT_TRUE(form.key(Key::Down));

    EXPECT_EQ(form.view(), View::Picker);
}

TEST(SettingsFormTest, typing_searches_the_families_ignoring_the_case)
{
    SettingsForm form = make();
    form.open_picker();

    EXPECT_TRUE(form.type("MONO"));

    EXPECT_EQ(form.query(), "MONO");
    EXPECT_EQ(form.families(), (Strings{"Beta Mono", "Delta Mono"}));
    EXPECT_EQ(form.selected(), 0u);
    // the font of the form is not touched by the search
    EXPECT_EQ(form.text(Field::Font), "Gamma");
}

TEST(SettingsFormTest, backspace_shortens_the_search_and_escape_clears_it)
{
    SettingsForm form = make();
    form.open_picker();
    form.type("mo");

    EXPECT_TRUE(form.key(Key::Back));
    EXPECT_EQ(form.query(), "m");
    EXPECT_TRUE(form.key(Key::Escape)); // clears the search first
    EXPECT_TRUE(form.query().empty());
    EXPECT_EQ(form.view(), View::Picker);
    EXPECT_EQ(form.families().size(), families.size());
    EXPECT_FALSE(form.key(Key::Back)); // nothing to delete
}

TEST(SettingsFormTest, escape_goes_back_and_leaves_the_font_as_it_was)
{
    SettingsForm form = make();
    form.open_picker();
    form.key(Key::Down);

    EXPECT_TRUE(form.key(Key::Escape));

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.text(Field::Font), "Gamma");
    EXPECT_EQ(form.result(), Result::Editing);
}

TEST(SettingsFormTest, return_takes_the_selected_family_into_the_form)
{
    SettingsForm form = make();
    form.open_picker();
    form.type("zet");

    EXPECT_TRUE(form.key(Key::Return));

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.text(Field::Font), "Zeta");
    EXPECT_EQ(form.focus(), Field::Font);
    EXPECT_EQ(form.result(), Result::Editing);
}

TEST(SettingsFormTest, return_in_a_search_without_a_result_keeps_the_font)
{
    SettingsForm form = make();
    form.open_picker();
    form.type("no such family");

    EXPECT_TRUE(form.families().empty());
    EXPECT_FALSE(form.key(Key::Down));
    form.key(Key::Return);

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.text(Field::Font), "Gamma");
}

TEST(SettingsFormTest, a_click_on_a_row_takes_the_family)
{
    SettingsForm form = make();
    form.open_picker();

    EXPECT_TRUE(click(form, form.row_rect(form.first() + 1)));

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.text(Field::Font), "Beta Mono");
}

TEST(SettingsFormTest, a_click_outside_of_the_rows_does_nothing)
{
    SettingsForm form = make();
    form.open_picker();

    // on the search field
    EXPECT_FALSE(click(form, form.search_rect()));
    // under the last row of a short list
    form.type("zeta");
    EXPECT_FALSE(click(form, form.row_rect(2)));
    EXPECT_EQ(form.view(), View::Picker);
}

TEST(SettingsFormTest, the_keys_move_the_selection_and_keep_it_in_view)
{
    SettingsForm form = make({"Gamma", "xterm -e", "1"});
    form.resize(500, 160); // few rows
    form.open_picker();
    const size_t rows = form.visible_rows();
    ASSERT_LT(rows, families.size());

    EXPECT_TRUE(form.key(Key::Home));
    EXPECT_EQ(form.selected(), 0u);
    EXPECT_FALSE(form.key(Key::Up)); // already the first
    EXPECT_FALSE(form.key(Key::Home));
    EXPECT_TRUE(form.key(Key::Down));
    EXPECT_EQ(form.selected(), 1u);
    EXPECT_TRUE(form.key(Key::PageDown));
    EXPECT_EQ(form.selected(), 1u + rows);
    EXPECT_LE(form.selected(), form.first() + rows - 1);
    EXPECT_TRUE(form.key(Key::PageUp));
    EXPECT_EQ(form.selected(), 1u);
    EXPECT_TRUE(form.key(Key::End));
    EXPECT_EQ(form.selected(), families.size() - 1);
    EXPECT_EQ(form.first(), families.size() - rows);
    EXPECT_FALSE(form.key(Key::Down)); // already the last
    EXPECT_TRUE(form.key(Key::Up));
    EXPECT_EQ(form.selected(), families.size() - 2);
    for (size_t i = 0; i < families.size(); ++i) {
        form.key(Key::PageUp); // the first page goes to the first row
    }
    EXPECT_EQ(form.selected(), 0u);
    EXPECT_EQ(form.first(), 0u);
}

TEST(SettingsFormTest, the_tab_does_nothing_in_the_list)
{
    SettingsForm form = make();
    form.open_picker();

    EXPECT_FALSE(form.key(Key::Tab));
    EXPECT_FALSE(form.key(Key::BackTab));
}

TEST(SettingsFormTest, the_wheel_scrolls_the_rows_but_not_the_selection)
{
    SettingsForm form = make();
    form.resize(500, 160);
    form.open_picker();
    form.key(Key::Home);
    const size_t last_first = families.size() - form.visible_rows();

    EXPECT_FALSE(form.wheel(-1)); // already at the top
    EXPECT_TRUE(form.wheel(1));
    EXPECT_GT(form.first(), 0u);
    EXPECT_EQ(form.selected(), 0u);
    EXPECT_TRUE(form.wheel(100)); // not further than the last page
    EXPECT_EQ(form.first(), last_first);
    EXPECT_FALSE(form.wheel(1));
    EXPECT_TRUE(form.wheel(-100));
    EXPECT_EQ(form.first(), 0u);
}

TEST(SettingsFormTest, the_rows_are_under_the_search_field_one_under_another)
{
    SettingsForm form = make();
    form.open_picker();

    const Rect search = form.search_rect();
    const Rect row0 = form.row_rect(0);
    const Rect row1 = form.row_rect(1);
    EXPECT_GE(row0.y, search.y + search.height);
    EXPECT_EQ(row1.y, row0.y + row0.height);
    // the rows after the first shown one are below it, scrolled ones above
    form.resize(500, 160);
    form.key(Key::End);
    EXPECT_LT(form.row_rect(0).y, 0);
}

TEST(SettingsFormTest, a_smaller_window_keeps_the_selection_in_view)
{
    SettingsForm form = make({"Kappa", "xterm -e", "1"});
    form.open_picker();
    ASSERT_EQ(form.selected(), 9u);

    form.resize(500, 160);

    EXPECT_GE(form.selected(), form.first());
    EXPECT_LT(form.selected(), form.first() + form.visible_rows());
}

TEST(SettingsFormTest, opening_again_forgets_the_old_search)
{
    SettingsForm form = make();
    form.open_picker();
    form.type("zeta");
    form.key(Key::Return);

    form.open_picker();

    EXPECT_TRUE(form.query().empty());
    EXPECT_EQ(form.families()[form.selected()], "Zeta");
}

TEST(SettingsFormTest, a_list_without_fonts_can_be_opened_and_closed)
{
    SettingsForm form = make({"Gamma", "xterm -e", "1"}, {});

    form.open_picker();
    EXPECT_TRUE(form.families().empty());
    EXPECT_FALSE(form.wheel(1));
    EXPECT_FALSE(form.key(Key::Home));
    form.choose();

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.text(Field::Font), "Gamma");
}

TEST(SettingsFormTest, a_click_under_the_last_whole_row_does_nothing)
{
    SettingsForm form = make();
    form.resize(500, 160); // two whole rows and a strip that is not a row
    form.open_picker();
    ASSERT_EQ(form.visible_rows(), 2u);

    const Rect strip = form.row_rect(form.first() + form.visible_rows());
    EXPECT_FALSE(click(form, {strip.x, strip.y, strip.width, 5}));
    EXPECT_EQ(form.view(), View::Picker);
}
