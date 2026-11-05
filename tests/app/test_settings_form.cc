#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "app/settings_form.h"

using tiny::Rect;
using tiny::app::SettingsForm;
using Field = SettingsForm::Field;
using Key = SettingsForm::Key;
using Result = SettingsForm::Result;
using View = SettingsForm::View;
using Pane = SettingsForm::Pane;
using Strings = std::vector<std::string>;

namespace {

const Strings families = {"Alpha",   "Beta Mono", "Gamma", "Delta Mono",
                          "Epsilon", "Zeta",      "Eta",   "Theta",
                          "Iota",    "Kappa"};

SettingsForm::Values V(const char* font, const char* terminal,
                       const char* scale)
{
    return {font, terminal, scale, "12", "", "", "", ""};
}

SettingsForm make(const SettingsForm::Values& values = V("Gamma", "xterm -e",
                                                         "1"),
                  const Strings& list = families)
{
    SettingsForm form(values, list, SettingsForm::Metrics());
    form.resize(500, 600);
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

//! The form with the focus on a field: its section shows, then Tab
void focus_on(SettingsForm& form, Field field)
{
    form.set_section(SettingsForm::section_of(field));
    while (form.focus() != field) {
        form.key(Key::Tab);
    }
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
    const Rect size = form.field_rect(Field::Size);
    const Rect rgba = form.field_rect(Field::Rgba);
    EXPECT_LT(font.y + font.height, size.y);
    EXPECT_LT(size.y + size.height, rgba.y);
    EXPECT_EQ(form.label_rect(Field::Size).y, size.y);
    // the font field leaves room for the button that opens the list
    EXPECT_LT(font.x + font.width, form.choose_rect().x);
    EXPECT_EQ(size.x + size.width,
              form.choose_rect().x + form.choose_rect().width);
    // the buttons are at the bottom, Cancel left of Save
    EXPECT_LT(form.cancel_rect().x, form.save_rect().x);
    EXPECT_GT(form.save_rect().y, rgba.y);
    EXPECT_LE(form.save_rect().y + form.save_rect().height, 600);
    EXPECT_GT(form.message_rect().y, rgba.y);
    EXPECT_LT(form.message_rect().y + form.message_rect().height,
              form.save_rect().y + form.save_rect().height);
}

TEST(SettingsFormTest, the_fields_are_right_of_the_panel_with_the_sections)
{
    const SettingsForm form = make();

    const int panel = form.sidebar_rect().width;
    EXPECT_GT(panel, 0);
    EXPECT_EQ(form.sidebar_rect().height, 600);
    for (const Field f : {Field::Font, Field::Scale, Field::Terminal}) {
        EXPECT_GE(form.label_rect(f).x, panel);
        EXPECT_GE(form.field_rect(f).x, form.label_rect(f).x);
    }
    EXPECT_GT(form.message_rect().x, panel);
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
    EXPECT_GE(form.section_rect(SettingsForm::Section::Fonts).height, 1);
    EXPECT_GE(form.search_rect().width, 1);
    EXPECT_GE(form.visible_rows(), 1u);
}

TEST(SettingsFormTest, typing_goes_to_the_focused_field)
{
    SettingsForm form = make();

    EXPECT_TRUE(form.type("X"));
    EXPECT_EQ(form.text(Field::Font), "GammaX");
    focus_on(form, Field::Terminal);
    EXPECT_TRUE(form.type(" -x"));
    EXPECT_EQ(form.text(Field::Terminal), "xterm -e -x");
    focus_on(form, Field::Scale);
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
    SettingsForm form = make(V("", "", ""));

    EXPECT_FALSE(form.key(Key::Back)); // nothing to delete
    form.type("a\xC5\x99");            // a, r with a caron
    EXPECT_TRUE(form.key(Key::Back));
    EXPECT_EQ(form.text(Field::Font), "a");
    EXPECT_TRUE(form.key(Key::Back));
    EXPECT_TRUE(form.text(Field::Font).empty());
}

TEST(SettingsFormTest, tab_goes_round_the_fields_of_the_section_and_back)
{
    SettingsForm form = make();

    for (const Field next : {Field::Size, Field::Style, Field::Antialias,
                             Field::Hinting, Field::Rgba, Field::Font}) {
        form.key(Key::Tab);
        EXPECT_EQ(form.focus(), next);
    }
    form.key(Key::BackTab);
    EXPECT_EQ(form.focus(), Field::Rgba);
    form.key(Key::BackTab);
    EXPECT_EQ(form.focus(), Field::Hinting);
}

TEST(SettingsFormTest, a_section_with_one_field_keeps_the_focus_on_it)
{
    SettingsForm form = make();
    form.set_section(SettingsForm::Section::Applications);

    form.key(Key::Tab);
    EXPECT_EQ(form.focus(), Field::Terminal);
    form.key(Key::BackTab);
    EXPECT_EQ(form.focus(), Field::Terminal);
}

TEST(SettingsFormTest, the_arrows_move_between_the_fields)
{
    SettingsForm form = make();

    form.key(Key::Up); // round to the last of the section
    EXPECT_EQ(form.focus(), Field::Rgba);
    form.key(Key::Down); // round to the first
    EXPECT_EQ(form.focus(), Field::Font);
    EXPECT_EQ(form.view(), View::Form);
    form.key(Key::Tab);
    form.key(Key::Down);
    EXPECT_EQ(form.focus(), Field::Style);
    form.key(Key::Up);
    EXPECT_EQ(form.focus(), Field::Size);
}

TEST(SettingsFormTest, the_page_keys_do_nothing_in_the_form)
{
    SettingsForm form = make();

    for (const Key key : {Key::PageUp, Key::PageDown}) {
        EXPECT_FALSE(form.key(key));
    }
}

TEST(SettingsFormTest, a_click_into_a_field_focuses_it)
{
    SettingsForm form = make();

    EXPECT_TRUE(click(form, form.field_rect(Field::Style)));
    EXPECT_EQ(form.focus(), Field::Style);
    EXPECT_TRUE(click(form, form.field_rect(Field::Size)));
    EXPECT_EQ(form.focus(), Field::Size);
    EXPECT_TRUE(click(form, form.field_rect(Field::Font)));
    EXPECT_EQ(form.focus(), Field::Font);
}

TEST(SettingsFormTest, a_press_that_moves_away_before_the_release_does_nothing)
{
    SettingsForm form = make();

    const Rect scale = form.field_rect(Field::Size);
    form.press(center_x(scale), center_y(scale));
    const Rect terminal = form.field_rect(Field::Style);
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
    SettingsForm form = make(V("", "", "big"));

    EXPECT_FALSE(form.problem(Field::Font).empty());
    EXPECT_FALSE(form.problem(Field::Terminal).empty());
    EXPECT_FALSE(form.problem(Field::Scale).empty());
    EXPECT_EQ(form.problem(), form.problem(Field::Font));

    form.type("Gamma");
    EXPECT_EQ(form.problem(), form.problem(Field::Terminal));
    focus_on(form, Field::Terminal);
    form.type("xterm -e");
    EXPECT_TRUE(form.problem(Field::Terminal).empty());
    EXPECT_EQ(form.problem(), form.problem(Field::Scale));
}

TEST(SettingsFormTest, a_blank_terminal_is_a_problem)
{
    const SettingsForm form = make(V("Gamma", "   ", "1"));

    EXPECT_FALSE(form.problem(Field::Terminal).empty());
}

TEST(SettingsFormTest, the_values_to_write_have_no_blanks_at_the_ends)
{
    const SettingsForm form = make(V(" Gamma ", " foot -e", "2 "));

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
    SettingsForm form = make(V("Gamma", "xterm -e", "0"));

    click(form, form.save_rect()); // shows the field that is wrong
    EXPECT_EQ(form.result(), Result::Editing);
    EXPECT_FALSE(form.key(Key::Return)); // already there
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
    SettingsForm form = make(V("gamma", "xterm -e", "1")); // another case

    EXPECT_TRUE(click(form, form.choose_rect()));

    EXPECT_EQ(form.view(), View::Picker);
    EXPECT_EQ(form.families().size(), families.size());
    EXPECT_EQ(form.families()[form.selected()], "Gamma");
    EXPECT_TRUE(form.query().empty());
}

TEST(SettingsFormTest, the_list_starts_at_the_top_for_a_font_that_is_not_there)
{
    SettingsForm form = make(V("Nothing", "xterm -e", "1"));

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

TEST(SettingsFormTest, a_click_on_a_row_selects_it_and_select_takes_it)
{
    SettingsForm form = make();
    form.open_picker();

    EXPECT_TRUE(click(form, form.row_rect(form.first() + 1)));

    EXPECT_EQ(form.view(), View::Picker);
    EXPECT_EQ(form.preview_family(), "Beta Mono");
    EXPECT_TRUE(click(form, form.save_rect()));
    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.text(Field::Font), "Beta Mono");
}

TEST(SettingsFormTest, a_click_outside_of_the_rows_does_nothing)
{
    SettingsForm form = make();
    form.open_picker();

    // between the search field and the first row
    form.press(10, form.search_rect().y + form.search_rect().height + 3);
    EXPECT_FALSE(form.release(10, form.search_rect().y +
                                          form.search_rect().height + 3));
    // under the last row of a short list
    form.type("zeta");
    EXPECT_FALSE(click(form, form.row_rect(2)));
    EXPECT_EQ(form.view(), View::Picker);
}

TEST(SettingsFormTest, the_keys_move_the_selection_and_keep_it_in_view)
{
    SettingsForm form = make(V("Gamma", "xterm -e", "1"));
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

TEST(SettingsFormTest, tab_goes_round_the_search_the_size_and_the_style)
{
    SettingsForm form = make();
    form.open_picker();
    EXPECT_EQ(form.pane(), Pane::Search);

    form.key(Key::Tab);
    EXPECT_EQ(form.pane(), Pane::Size);
    form.key(Key::Tab);
    EXPECT_EQ(form.pane(), Pane::Style);
    form.key(Key::Tab);
    EXPECT_EQ(form.pane(), Pane::Search);
    form.key(Key::BackTab);
    EXPECT_EQ(form.pane(), Pane::Style);
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
    SettingsForm form = make(V("Kappa", "xterm -e", "1"));
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
    SettingsForm form = make(V("Gamma", "xterm -e", "1"), {});

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
    form.resize(500, 372); // two whole rows and a strip that is not a row
    form.open_picker();
    ASSERT_EQ(form.visible_rows(), 2u);

    const Rect strip = form.row_rect(form.first() + form.visible_rows());
    EXPECT_FALSE(click(form, {strip.x, strip.y, strip.width, 5}));
    EXPECT_EQ(form.view(), View::Picker);
}

// The cursor

TEST(SettingsFormTest, the_arrows_move_the_cursor_in_a_field)
{
    SettingsForm form = make(V("Gamma", "xterm -e", "1"));

    EXPECT_EQ(form.field(Field::Font).cursor(), 5u); // at the end
    EXPECT_TRUE(form.key(Key::Left));
    EXPECT_TRUE(form.key(Key::Left));
    EXPECT_EQ(form.field(Field::Font).cursor(), 3u);
    EXPECT_TRUE(form.type("--"));
    EXPECT_EQ(form.text(Field::Font), "Gam--ma");
    EXPECT_TRUE(form.key(Key::Right));
    EXPECT_EQ(form.field(Field::Font).cursor(), 6u);
    EXPECT_TRUE(form.key(Key::Home));
    EXPECT_FALSE(form.key(Key::Left));
    EXPECT_FALSE(form.key(Key::Home));
    EXPECT_TRUE(form.key(Key::End));
    EXPECT_FALSE(form.key(Key::Right));
    EXPECT_FALSE(form.key(Key::End));
}

TEST(SettingsFormTest, delete_takes_the_character_after_the_cursor)
{
    SettingsForm form = make(V("Gamma", "xterm -e", "1"));

    EXPECT_FALSE(form.key(Key::Delete)); // at the end
    form.key(Key::Home);
    EXPECT_TRUE(form.key(Key::Delete));
    EXPECT_EQ(form.text(Field::Font), "amma");
    form.key(Key::Right);
    EXPECT_TRUE(form.key(Key::Back)); // before the cursor
    EXPECT_EQ(form.text(Field::Font), "mma");
}

TEST(SettingsFormTest, an_edit_takes_the_message_of_a_failed_write_away)
{
    SettingsForm form = make();
    form.reject("Not saved");
    form.key(Key::Delete); // at the end: nothing changes
    EXPECT_FALSE(form.status().empty());
    form.key(Key::Left); // a move is not an edit
    EXPECT_FALSE(form.status().empty());
    form.key(Key::Delete);
    EXPECT_TRUE(form.status().empty());
}

//! Every byte is 10 pixels wide
SettingsForm make_measured()
{
    SettingsForm form = make(V("Gamma", "xterm -e", "1"));
    form.set_measure([](const std::string& text) {
        return static_cast<int>(text.size()) * 10;
    });
    return form;
}

TEST(SettingsFormTest, a_click_puts_the_cursor_where_the_pointer_is)
{
    SettingsForm form = make_measured();
    form.set_section(SettingsForm::Section::Applications);
    const Rect r = form.field_rect(Field::Terminal);
    const int start = r.x + SettingsForm::Metrics().padding;

    // the nearest border: 22 is nearer to 20 (2 characters) than to 30
    form.press(start + 22, center_y(r));
    EXPECT_TRUE(form.release(start + 22, center_y(r)));
    EXPECT_EQ(form.focus(), Field::Terminal);
    EXPECT_EQ(form.field(Field::Terminal).cursor(), 2u);

    form.press(start + 26, center_y(r));
    form.release(start + 26, center_y(r));
    EXPECT_EQ(form.field(Field::Terminal).cursor(), 3u);

    // before the text and after it
    form.press(r.x + 1, center_y(r));
    form.release(r.x + 1, center_y(r));
    EXPECT_EQ(form.field(Field::Terminal).cursor(), 0u);
    form.press(r.x + r.width - 2, center_y(r));
    form.release(r.x + r.width - 2, center_y(r));
    EXPECT_EQ(form.field(Field::Terminal).cursor(), 8u);
}

TEST(SettingsFormTest, a_click_in_a_long_text_counts_the_shift_of_the_text)
{
    SettingsForm form = make_measured();
    const Rect r = form.field_rect(Field::Font);
    const int room = r.width - 2 * SettingsForm::Metrics().padding;
    // the text is longer than the field and the cursor is at its end
    form.type(std::string(static_cast<size_t>(room / 10 + 5), 'x'));
    const tiny::TextField& font = form.field(Field::Font);
    const int shift = form.shift(font, room);
    ASSERT_LT(shift, 0);

    const int start = r.x + SettingsForm::Metrics().padding + shift;
    form.press(start + 100, center_y(r));
    form.release(start + 100, center_y(r));

    EXPECT_EQ(form.field(Field::Font).cursor(), 10u);
}

TEST(SettingsFormTest, without_a_measure_the_text_does_not_shift)
{
    const SettingsForm form = make();

    EXPECT_EQ(form.shift(form.field(Field::Font), 10), 0);
}

TEST(SettingsFormTest, the_shift_keeps_the_cursor_in_the_field)
{
    SettingsForm form = make_measured(); // "Gamma", 50 pixels

    EXPECT_EQ(form.shift(form.field(Field::Font), 100), 0);
    EXPECT_EQ(form.shift(form.field(Field::Font), 30), -20);
    form.key(Key::Home);
    EXPECT_EQ(form.shift(form.field(Field::Font), 30), 0);
}

// The size, the style and the preview

namespace {

//! The styles of Beta Mono and Gamma
Strings styles_of(const std::string& family)
{
    if (family == "Gamma") {
        return {"Regular", "Bold", "Italic"};
    }
    if (family == "Beta Mono") {
        return {"Book"};
    }
    return {};
}

SettingsForm make_with_styles(const std::string& style = "")
{
    SettingsForm form({"Gamma", "xterm -e", "1", "12", style, "", "", ""},
                      families, SettingsForm::Metrics(), styles_of);
    form.resize(500, 600);
    return form;
}

} // namespace

TEST(SettingsFormTest, the_form_has_the_size_and_the_style)
{
    SettingsForm form = make(
            {"Gamma", "xterm -e", "1", "14", "Bold", "", "", ""}, families);

    EXPECT_EQ(form.text(Field::Size), "14");
    EXPECT_EQ(form.text(Field::Style), "Bold");
    EXPECT_EQ(form.values().size, "14");
    EXPECT_EQ(form.values().style, "Bold");
    EXPECT_EQ(form.clean_values().size, "14");
}

TEST(SettingsFormTest, a_size_that_is_not_a_number_in_the_limits_is_a_problem)
{
    for (const char* size : {"", "3", "97", "big", "12pt"}) {
        const SettingsForm form =
                make({"Gamma", "xterm -e", "1", size, "", "", "", ""});
        EXPECT_FALSE(form.problem(Field::Size).empty()) << size;
    }
    for (const char* size : {"4", "9.5", " 12 ", "96"}) {
        const SettingsForm form =
                make({"Gamma", "xterm -e", "1", size, "", "", "", ""});
        EXPECT_TRUE(form.problem(Field::Size).empty()) << size;
    }
}

TEST(SettingsFormTest, the_style_may_be_empty_but_not_have_a_control_character)
{
    EXPECT_TRUE(make({"Gamma", "xterm -e", "1", "12", "", "", "", ""})
                        .problem(Field::Style)
                        .empty());
    EXPECT_FALSE(make({"Gamma", "xterm -e", "1", "12", "a\tb", "", "", ""})
                         .problem(Field::Style)
                         .empty());
}

TEST(SettingsFormTest, the_list_starts_with_the_size_and_the_style_of_the_form)
{
    SettingsForm form({"Gamma", "xterm -e", "1", "14", "Bold", "", "", ""},
                      families, SettingsForm::Metrics(), styles_of);
    form.resize(500, 600);

    form.open_picker();

    EXPECT_EQ(form.pane(), Pane::Search);
    EXPECT_EQ(form.size_field().text(), "14");
    EXPECT_EQ(form.pick_style(), "Bold");
    EXPECT_EQ(form.pick_styles(), (Strings{"Regular", "Bold", "Italic"}));
    EXPECT_EQ(form.preview_family(), "Gamma");
    EXPECT_EQ(form.preview_size(), 14.0);
}

TEST(SettingsFormTest, a_family_without_the_style_has_the_default_one)
{
    SettingsForm form = make_with_styles("Bold");
    form.open_picker();
    ASSERT_EQ(form.pick_style(), "Bold");

    form.type("beta"); // Beta Mono has only Book

    EXPECT_EQ(form.pick_styles(), (Strings{"Book"}));
    EXPECT_TRUE(form.pick_style().empty());
}

TEST(SettingsFormTest, a_family_that_has_no_styles_is_shown_with_the_default)
{
    SettingsForm form = make_with_styles();
    form.open_picker();

    form.type("zeta");

    EXPECT_TRUE(form.pick_styles().empty());
    EXPECT_TRUE(form.pick_style().empty());
}

TEST(SettingsFormTest, the_styles_of_a_family_are_asked_for_once)
{
    int asked = 0;
    SettingsForm form({"Gamma", "xterm -e", "1", "12", "", "", "", ""},
                      families, SettingsForm::Metrics(),
                      [&](const std::string&) -> Strings {
                          ++asked;
                          return {"Regular"};
                      });
    form.resize(500, 600);
    form.open_picker();
    const int after_open = asked;

    form.key(Key::Down);
    form.key(Key::Up);
    form.key(Key::Down);
    form.key(Key::Up);

    EXPECT_EQ(asked, after_open + 1); // the neighbor, once
}

TEST(SettingsFormTest, the_style_is_chosen_in_the_list_of_the_styles)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);
    form.key(Key::Tab);
    ASSERT_EQ(form.pane(), Pane::Style);

    EXPECT_TRUE(form.key(Key::Down));
    EXPECT_EQ(form.pick_style(), "Regular");
    EXPECT_TRUE(form.key(Key::Right));
    EXPECT_EQ(form.pick_style(), "Bold");
    EXPECT_TRUE(form.key(Key::Down));
    EXPECT_EQ(form.pick_style(), "Italic");
    EXPECT_TRUE(form.key(Key::Down)); // round to the default
    EXPECT_TRUE(form.pick_style().empty());
    EXPECT_TRUE(form.key(Key::Up)); // and back
    EXPECT_EQ(form.pick_style(), "Italic");
    EXPECT_TRUE(form.key(Key::Left));
    EXPECT_EQ(form.pick_style(), "Bold");
    EXPECT_FALSE(form.key(Key::Delete)); // nothing to edit
    EXPECT_FALSE(form.type("x"));
}

TEST(SettingsFormTest, a_click_on_the_style_takes_the_next_one)
{
    SettingsForm form = make_with_styles();
    form.open_picker();

    EXPECT_TRUE(click(form, form.style_rect()));
    EXPECT_EQ(form.pane(), Pane::Style);
    EXPECT_EQ(form.pick_style(), "Regular");
    click(form, form.style_rect());
    EXPECT_EQ(form.pick_style(), "Bold");
}

TEST(SettingsFormTest, a_style_box_without_choices_stays_default)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.type("zeta");

    EXPECT_FALSE(click(form, form.style_rect()));
    EXPECT_TRUE(form.pick_style().empty());
}

TEST(SettingsFormTest, up_and_down_change_the_size_in_the_size_field)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);
    ASSERT_EQ(form.pane(), Pane::Size);

    EXPECT_TRUE(form.key(Key::Up));
    EXPECT_EQ(form.size_field().text(), "13");
    EXPECT_TRUE(form.key(Key::Down));
    EXPECT_TRUE(form.key(Key::Down));
    EXPECT_EQ(form.size_field().text(), "11");
    EXPECT_EQ(form.preview_size(), 11.0);
    // the family is not changed by it
    EXPECT_EQ(form.preview_family(), "Gamma");
}

TEST(SettingsFormTest, the_size_stays_in_the_limits)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);
    form.key(Key::Home);
    form.key(Key::Delete);
    form.key(Key::Delete);
    form.type("96");

    EXPECT_FALSE(form.key(Key::Up));
    EXPECT_EQ(form.size_field().text(), "96");
    form.key(Key::Home);
    form.key(Key::Delete);
    form.key(Key::Delete);
    form.type("4");
    EXPECT_FALSE(form.key(Key::Down));
    EXPECT_EQ(form.size_field().text(), "4");
}

TEST(SettingsFormTest, a_wrong_size_steps_from_the_size_of_the_form)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);
    form.key(Key::Back);
    form.key(Key::Back); // empty
    ASSERT_FALSE(form.pick_problem().empty());
    EXPECT_EQ(form.preview_size(), 12.0);

    EXPECT_TRUE(form.key(Key::Up));

    EXPECT_EQ(form.size_field().text(), "13");
    EXPECT_TRUE(form.pick_problem().empty());
}

TEST(SettingsFormTest, only_a_number_is_typed_into_the_size)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);
    form.key(Key::End);

    EXPECT_FALSE(form.type("pt"));
    EXPECT_TRUE(form.type("a.5b"));

    EXPECT_EQ(form.size_field().text(), "12.5");
    EXPECT_EQ(form.preview_size(), 12.5);
}

TEST(SettingsFormTest, the_size_has_a_cursor_too)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);

    form.key(Key::Left);
    form.type("0");

    EXPECT_EQ(form.size_field().text(), "102");
    // the page keys still scroll the list
    EXPECT_TRUE(form.key(Key::PageDown));
}

TEST(SettingsFormTest, a_click_on_the_size_puts_the_cursor_there)
{
    SettingsForm form = make_with_styles();
    form.set_measure([](const std::string& text) {
        return static_cast<int>(text.size()) * 10;
    });
    form.open_picker();
    const Rect r = form.size_rect();

    form.press(r.x + SettingsForm::Metrics().padding + 11, center_y(r));
    form.release(r.x + SettingsForm::Metrics().padding + 11, center_y(r));

    EXPECT_EQ(form.pane(), Pane::Size);
    EXPECT_EQ(form.size_field().cursor(), 1u);
}

TEST(SettingsFormTest, the_search_has_a_cursor_too)
{
    SettingsForm form = make_with_styles();
    form.set_measure([](const std::string& text) {
        return static_cast<int>(text.size()) * 10;
    });
    form.open_picker();
    form.type("eta");
    ASSERT_EQ(form.families().size(), 4u); // Beta Mono, Zeta, Eta, Theta

    form.key(Key::Left);
    form.key(Key::Left);
    form.type("x"); // "exta": nothing matches
    EXPECT_TRUE(form.families().empty());
    form.key(Key::Back); // "eta" again
    EXPECT_FALSE(form.families().empty());
    form.key(Key::Home); // Home goes to the first family, not in the text
    EXPECT_EQ(form.query(), "eta");

    const Rect r = form.search_rect();
    form.press(r.x + SettingsForm::Metrics().padding + 11, center_y(r));
    form.release(r.x + SettingsForm::Metrics().padding + 11, center_y(r));
    EXPECT_EQ(form.search_field().cursor(), 1u);
}

TEST(SettingsFormTest, select_takes_the_family_the_size_and_the_style)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.type("gam");
    form.key(Key::Tab);
    form.key(Key::Up);
    form.key(Key::Tab);
    form.key(Key::Right);
    form.key(Key::Right);

    EXPECT_TRUE(form.key(Key::Return));

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.text(Field::Font), "Gamma");
    EXPECT_EQ(form.text(Field::Size), "13");
    EXPECT_EQ(form.text(Field::Style), "Bold");
}

TEST(SettingsFormTest, cancel_leaves_the_size_and_the_style_as_they_were)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);
    form.key(Key::Up);
    form.key(Key::Tab);
    form.key(Key::Right);

    EXPECT_TRUE(click(form, form.cancel_rect()));

    EXPECT_EQ(form.view(), View::Form);
    EXPECT_EQ(form.result(), Result::Editing);
    EXPECT_EQ(form.text(Field::Size), "12");
    EXPECT_TRUE(form.text(Field::Style).empty());
    // Escape does the same
    form.open_picker();
    form.key(Key::Tab);
    form.key(Key::Up);
    form.key(Key::Escape);
    EXPECT_EQ(form.text(Field::Size), "12");
}

TEST(SettingsFormTest, select_waits_for_a_right_size)
{
    SettingsForm form = make_with_styles();
    form.open_picker();
    form.key(Key::Tab);
    form.key(Key::Back);
    form.key(Key::Back);

    form.key(Key::Return);
    EXPECT_EQ(form.view(), View::Picker);
    EXPECT_FALSE(click(form, form.save_rect()) && form.view() == View::Form);
    EXPECT_EQ(form.view(), View::Picker);
    EXPECT_FALSE(form.choose());
}

TEST(SettingsFormTest, the_preview_is_over_the_controls_and_under_the_list)
{
    const SettingsForm form = make_with_styles();

    const Rect preview = form.preview_rect();
    const Rect size = form.size_rect();
    const Rect style = form.style_rect();
    EXPECT_LT(preview.y + preview.height, size.y);
    EXPECT_EQ(size.y, style.y);
    EXPECT_LT(size.x + size.width, form.style_label_rect().x);
    EXPECT_LT(form.style_label_rect().x, style.x);
    EXPECT_EQ(form.size_label_rect().y, size.y);
    EXPECT_LT(size.y + size.height, form.save_rect().y);
    EXPECT_NE(std::string(SettingsForm::preview_text()).find("Lorem ipsum"),
              std::string::npos);
    // the last row of the list is over the preview
    SettingsForm list = make_with_styles();
    list.open_picker();
    const Rect last = list.row_rect(list.first() + list.visible_rows() - 1);
    EXPECT_LE(last.y + last.height, preview.y);
}

namespace {

SettingsForm::Values with_rendering(const char* antialias, const char* hinting,
                                    const char* rgba)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.antialias = antialias;
    v.hinting = hinting;
    v.rgba = rgba;
    return v;
}

} // namespace

TEST(SettingsFormTest, the_rendering_of_text_has_choices_under_the_fields)
{
    const SettingsForm form = make(with_rendering("yes", "slight", "rgb"));

    EXPECT_TRUE(SettingsForm::is_choice(Field::Antialias));
    EXPECT_TRUE(SettingsForm::is_choice(Field::Hinting));
    EXPECT_TRUE(SettingsForm::is_choice(Field::Rgba));
    EXPECT_FALSE(SettingsForm::is_choice(Field::Scale));
    const auto v = form.values();
    EXPECT_EQ(v.antialias, "yes");
    EXPECT_EQ(v.hinting, "slight");
    EXPECT_EQ(v.rgba, "rgb");
    EXPECT_EQ(form.choice_label(Field::Antialias), "On");
    EXPECT_EQ(form.choice_label(Field::Hinting), "Slight");
    EXPECT_EQ(form.choice_label(Field::Rgba), "RGB");
    EXPECT_EQ(form.clean_values().rgba, "rgb");
    EXPECT_GT(form.field_rect(Field::Antialias).y,
              form.field_rect(Field::Scale).y);
    EXPECT_GT(form.message_rect().y, form.field_rect(Field::WallpaperMode).y);
    EXPECT_EQ(SettingsForm::xft_option(Field::Hinting),
              tiny::wm::XftOption::Hinting);
    EXPECT_EQ(SettingsForm::xft_option(Field::Antialias),
              tiny::wm::XftOption::Antialias);
}

TEST(SettingsFormTest, a_value_that_is_not_set_is_the_default)
{
    const SettingsForm form = make();

    EXPECT_EQ(form.choice_label(Field::Antialias), "Default");
    EXPECT_TRUE(form.problem().empty());
}

TEST(SettingsFormTest, a_value_that_is_not_offered_is_a_problem_and_is_shown)
{
    const SettingsForm form = make(with_rendering("maybe", "", ""));

    EXPECT_FALSE(form.problem(Field::Antialias).empty());
    EXPECT_EQ(form.problem(), form.problem(Field::Antialias));
    EXPECT_EQ(form.choice_label(Field::Antialias), "maybe");
}

TEST(SettingsFormTest, the_arrows_go_through_the_values_of_a_choice)
{
    SettingsForm form = make();
    focus_on(form, Field::Antialias);

    EXPECT_TRUE(form.key(Key::Right));
    EXPECT_EQ(form.text(Field::Antialias), "yes");
    EXPECT_TRUE(form.key(Key::Right));
    EXPECT_EQ(form.text(Field::Antialias), "no");
    EXPECT_TRUE(form.key(Key::Right)); // round to the default
    EXPECT_EQ(form.text(Field::Antialias), "");
    EXPECT_TRUE(form.key(Key::Left)); // and back
    EXPECT_EQ(form.text(Field::Antialias), "no");
    EXPECT_EQ(form.focus(), Field::Antialias);
}

TEST(SettingsFormTest, space_goes_to_the_next_value_and_other_text_is_ignored)
{
    SettingsForm form = make();
    focus_on(form, Field::Hinting);

    EXPECT_TRUE(form.type(" "));
    EXPECT_EQ(form.text(Field::Hinting), "none");
    EXPECT_FALSE(form.type("x"));
    EXPECT_EQ(form.text(Field::Hinting), "none");
}

TEST(SettingsFormTest, backspace_and_delete_put_a_choice_back_to_the_default)
{
    SettingsForm form = make(with_rendering("", "", "bgr"));
    focus_on(form, Field::Rgba);

    form.reject("Not saved: nope");
    EXPECT_TRUE(form.key(Key::Back));
    EXPECT_EQ(form.text(Field::Rgba), "");
    EXPECT_TRUE(form.status().empty());
    EXPECT_FALSE(form.key(Key::Delete)); // nothing to change
    EXPECT_FALSE(form.key(Key::Home));
    EXPECT_FALSE(form.key(Key::End));
}

TEST(SettingsFormTest, a_click_goes_on_in_the_right_half_and_back_in_the_left)
{
    SettingsForm form = make();
    const Rect r = form.field_rect(Field::Rgba);

    EXPECT_TRUE(click(form, {center_x(r), r.y, r.width / 2, r.height}));
    EXPECT_EQ(form.focus(), Field::Rgba);
    EXPECT_EQ(form.text(Field::Rgba), "none");
    EXPECT_TRUE(click(form, {center_x(r), r.y, r.width / 2, r.height}));
    EXPECT_EQ(form.text(Field::Rgba), "rgb");
    EXPECT_TRUE(click(form, {r.x, r.y, r.width / 2, r.height}));
    EXPECT_EQ(form.text(Field::Rgba), "none");
}

TEST(SettingsFormTest, a_choice_that_was_changed_clears_the_failed_write)
{
    SettingsForm form = make();
    form.reject("Not saved: nope");
    focus_on(form, Field::Antialias);

    form.key(Key::Right);

    EXPECT_TRUE(form.status().empty());
}

namespace {

SettingsForm::Values with_wallpaper(const char* file, const char* mode)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.wallpaper = file;
    v.wallpaper_mode = mode;
    return v;
}

} // namespace

TEST(SettingsFormTest, the_wallpaper_is_a_path_and_a_choice_of_the_placement)
{
    SettingsForm form = make(with_wallpaper("~/a.jpg", "tile"));

    EXPECT_FALSE(SettingsForm::is_choice(Field::Wallpaper));
    EXPECT_TRUE(SettingsForm::is_choice(Field::WallpaperMode));
    EXPECT_EQ(form.values().wallpaper, "~/a.jpg");
    EXPECT_EQ(form.values().wallpaper_mode, "tile");
    EXPECT_EQ(form.choice_label(Field::WallpaperMode), "Tile");
    EXPECT_EQ(SettingsForm::section_of(Field::Wallpaper),
              SettingsForm::Section::Appearance);
    EXPECT_EQ(SettingsForm::section_of(Field::WallpaperMode),
              SettingsForm::Section::Appearance);

    // the arrows go through fill (""), center and tile
    focus_on(form, Field::WallpaperMode);
    form.key(Key::Right);
    EXPECT_EQ(form.text(Field::WallpaperMode), "");
    EXPECT_EQ(form.choice_label(Field::WallpaperMode), "Fill");
    form.key(Key::Right);
    EXPECT_EQ(form.text(Field::WallpaperMode), "center");
    form.key(Key::Left);
    form.key(Key::Left);
    EXPECT_EQ(form.text(Field::WallpaperMode), "tile");
}

TEST(SettingsFormTest, the_path_of_the_wallpaper_is_typed)
{
    SettingsForm form = make(with_wallpaper("", ""));

    focus_on(form, Field::Wallpaper);
    EXPECT_TRUE(form.type("/p/a.png"));
    EXPECT_EQ(form.clean_values().wallpaper, "/p/a.png");
    EXPECT_TRUE(form.problem().empty());
}

TEST(SettingsFormTest, a_path_with_a_line_break_cannot_be_saved)
{
    SettingsForm form = make(with_wallpaper("/a\nb", ""));

    EXPECT_FALSE(form.problem(Field::Wallpaper).empty());
    EXPECT_FALSE(form.problem().empty());
    form = make(with_wallpaper("pics/a.png", ""));
    EXPECT_FALSE(form.problem(Field::Wallpaper).empty());
    form = make(with_wallpaper("", "stretch"));
    EXPECT_FALSE(form.problem(Field::WallpaperMode).empty());
    EXPECT_FALSE(form.problem().empty());
}

TEST(SettingsFormTest, a_click_on_the_placement_steps_through_the_values)
{
    SettingsForm form = make(with_wallpaper("", ""));
    form.resize(500, form.form_height());
    form.set_section(SettingsForm::Section::Appearance);

    const Rect r = form.field_rect(Field::WallpaperMode);
    form.press(r.x + r.width - 2, r.y + 2);
    EXPECT_TRUE(form.release(r.x + r.width - 2, r.y + 2));
    EXPECT_EQ(form.focus(), Field::WallpaperMode);
    EXPECT_EQ(form.text(Field::WallpaperMode), "center");
}

// The sections

TEST(SettingsFormSectionTest, the_form_starts_in_the_fonts)
{
    const SettingsForm form = make();

    EXPECT_EQ(form.section(), SettingsForm::Section::Fonts);
}

TEST(SettingsFormSectionTest, every_field_is_in_one_section)
{
    using Section = SettingsForm::Section;
    size_t count = 0;
    for (const Section section :
         {Section::Fonts, Section::Appearance, Section::Applications}) {
        for (const Field f : SettingsForm::section_fields(section)) {
            EXPECT_EQ(SettingsForm::section_of(f), section);
            ++count;
        }
    }
    EXPECT_EQ(count, 12u);
}

TEST(SettingsFormSectionTest, the_sections_have_names)
{
    using Section = SettingsForm::Section;
    EXPECT_STREQ(SettingsForm::section_title(Section::Fonts), "Fonts");
    EXPECT_STREQ(SettingsForm::section_title(Section::Appearance),
                 "Appearance");
    EXPECT_STREQ(SettingsForm::section_title(Section::Applications),
                 "Applications");
}

TEST(SettingsFormSectionTest, a_click_on_an_item_of_the_panel_shows_its_section)
{
    SettingsForm form = make();

    EXPECT_TRUE(
            click(form, form.section_rect(SettingsForm::Section::Appearance)));

    EXPECT_EQ(form.section(), SettingsForm::Section::Appearance);
    EXPECT_EQ(form.focus(), Field::Scale);
    EXPECT_TRUE(click(form, form.field_rect(Field::ClockFormat)));
    EXPECT_EQ(form.focus(), Field::ClockFormat);
}

TEST(SettingsFormSectionTest, the_items_of_the_panel_are_one_under_another)
{
    const SettingsForm form = make();
    using Section = SettingsForm::Section;

    const Rect first = form.section_rect(Section::Fonts);
    const Rect second = form.section_rect(Section::Appearance);
    const Rect third = form.section_rect(Section::Applications);
    EXPECT_EQ(first.y + first.height, second.y);
    EXPECT_EQ(second.y + second.height, third.y);
    EXPECT_EQ(first.width, form.sidebar_rect().width);
}

TEST(SettingsFormSectionTest, ctrl_page_down_and_up_go_through_the_sections)
{
    SettingsForm form = make();
    using Section = SettingsForm::Section;

    EXPECT_TRUE(form.key(Key::NextSection));
    EXPECT_EQ(form.section(), Section::Appearance);
    EXPECT_TRUE(form.key(Key::NextSection));
    EXPECT_EQ(form.section(), Section::Applications);
    EXPECT_TRUE(form.key(Key::NextSection));
    EXPECT_EQ(form.section(), Section::Fonts); // round
    EXPECT_TRUE(form.key(Key::PrevSection));
    EXPECT_EQ(form.section(), Section::Applications);
}

TEST(SettingsFormSectionTest, a_section_puts_the_focus_on_its_first_field)
{
    SettingsForm form = make();

    form.set_section(SettingsForm::Section::Appearance);
    EXPECT_EQ(form.focus(), Field::Scale);
    form.set_section(SettingsForm::Section::Appearance); // the same one
    form.key(Key::Tab);
    form.set_section(SettingsForm::Section::Appearance);
    EXPECT_NE(form.focus(), Field::Scale); // not moved again
}

TEST(SettingsFormSectionTest, the_button_that_opens_the_fonts_is_in_the_fonts)
{
    SettingsForm form = make();
    const Rect choose = form.choose_rect();
    form.set_section(SettingsForm::Section::Appearance);

    // the place is a field of the other section, not the button
    form.press(center_x(choose), center_y(choose));
    form.release(center_x(choose), center_y(choose));

    EXPECT_EQ(form.view(), View::Form);
}

TEST(SettingsFormSectionTest, saving_with_a_problem_shows_the_field_with_it)
{
    SettingsForm form = make(V("Gamma", "xterm -e", "big"));

    EXPECT_TRUE(form.key(Key::Return));

    EXPECT_EQ(form.result(), Result::Editing);
    EXPECT_EQ(form.section(), SettingsForm::Section::Appearance);
    EXPECT_EQ(form.focus(), Field::Scale);
}

TEST(SettingsFormSectionTest, the_list_of_fonts_returns_to_the_fonts)
{
    SettingsForm form = make();
    form.open_picker();

    form.close_picker();

    EXPECT_EQ(form.section(), SettingsForm::Section::Fonts);
    EXPECT_EQ(form.focus(), Field::Font);
}

TEST(SettingsFormSectionTest, the_form_is_as_high_as_the_longest_section)
{
    SettingsForm form = make();
    form.resize(500, form.form_height());

    const Rect last = form.field_rect(Field::Rgba);
    EXPECT_GT(form.message_rect().y, last.y + last.height);
}

// The clock and the dock

TEST(SettingsFormClockDockTest, the_values_come_from_the_start)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.clock_format = "%R";
    v.dock = "on";
    const SettingsForm form = make(v);

    EXPECT_EQ(form.values().clock_format, "%R");
    EXPECT_EQ(form.values().dock, "on");
    EXPECT_EQ(form.choice_label(Field::Dock), "On");
    EXPECT_TRUE(SettingsForm::is_choice(Field::Dock));
    EXPECT_FALSE(SettingsForm::is_choice(Field::ClockFormat));
}

TEST(SettingsFormClockDockTest, the_dock_hides_by_default)
{
    const SettingsForm form = make();

    EXPECT_EQ(form.choice_label(Field::Dock), "Auto-hide");
}

TEST(SettingsFormClockDockTest, the_arrows_go_through_the_modes_of_the_dock)
{
    SettingsForm form = make();
    focus_on(form, Field::Dock);

    form.key(Key::Right);
    EXPECT_EQ(form.text(Field::Dock), "on");
    form.key(Key::Right);
    EXPECT_EQ(form.text(Field::Dock), "off");
    form.key(Key::Right);
    EXPECT_EQ(form.text(Field::Dock), "");
    form.key(Key::Left);
    EXPECT_EQ(form.text(Field::Dock), "off");
}

TEST(SettingsFormClockDockTest, a_wrong_mode_of_the_dock_is_a_problem)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.dock = "never";

    EXPECT_FALSE(make(v).problem(Field::Dock).empty());
    v.dock = "autohide"; // the file's name is not a value of the form
    EXPECT_FALSE(make(v).problem(Field::Dock).empty());
}

TEST(SettingsFormClockDockTest, the_format_of_the_clock_is_typed)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.clock_format = "";
    SettingsForm form = make(v);

    focus_on(form, Field::ClockFormat);
    EXPECT_TRUE(form.type("%a %R"));

    EXPECT_EQ(form.values().clock_format, "%a %R");
    EXPECT_TRUE(form.problem().empty());
}

TEST(SettingsFormClockDockTest, an_empty_format_means_no_clock_and_is_right)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.clock_format = "";

    EXPECT_TRUE(make(v).problem(Field::ClockFormat).empty());
}

TEST(SettingsFormClockDockTest, a_format_with_a_line_break_cannot_be_saved)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.clock_format = "%R\n%a";

    const SettingsForm form = make(v);

    EXPECT_FALSE(form.problem(Field::ClockFormat).empty());
    EXPECT_FALSE(form.problem().empty());
}

TEST(SettingsFormClockDockTest, the_blanks_of_the_format_are_kept)
{
    SettingsForm::Values v = V("Gamma", "xterm -e", "1");
    v.clock_format = " %R ";

    EXPECT_EQ(make(v).clean_values().clock_format, " %R ");
}

TEST(SettingsFormClockDockTest, some_fields_have_a_hint)
{
    EXPECT_NE(std::string(SettingsForm::hint(Field::ClockFormat)), "");
    EXPECT_NE(std::string(SettingsForm::hint(Field::Dock)), "");
    EXPECT_STREQ(SettingsForm::hint(Field::Size), "");
}

TEST(SettingsFormSectionTest, a_wrong_field_in_the_same_section_gets_the_focus)
{
    SettingsForm form = make(V("Gamma", "xterm -e", "1"));
    form.set_section(SettingsForm::Section::Fonts);
    form.key(Key::Tab); // Size
    for (const char c : std::string("abc")) {
        form.type(std::string(1, c)); // a size that is not a number
    }
    form.key(Key::Up); // Font: focus away from the wrong field

    EXPECT_TRUE(form.key(Key::Return)); // changed, so it is painted again
    EXPECT_EQ(form.focus(), Field::Size);
    EXPECT_EQ(form.result(), Result::Editing);
}

TEST(SettingsFormSectionTest, the_thumbnail_of_the_wallpaper_is_over_its_path)
{
    const SettingsForm form = make();

    const Rect thumb = form.thumbnail_rect();
    const Rect scale = form.field_rect(Field::Scale);
    const Rect path = form.field_rect(Field::Wallpaper);
    EXPECT_GT(thumb.y, scale.y + scale.height);
    EXPECT_LT(thumb.y + thumb.height, path.y);
    EXPECT_EQ(thumb.x, path.x);
    EXPECT_GE(thumb.width, 1);
    EXPECT_GE(thumb.height, 1);
}

TEST(SettingsFormSectionTest,
     a_tiny_window_has_a_thumbnail_of_one_pixel_at_least)
{
    SettingsForm form = make();
    form.resize(0, 0);

    EXPECT_GE(form.thumbnail_rect().width, 1);
    EXPECT_GE(form.thumbnail_rect().height, 1);
}
