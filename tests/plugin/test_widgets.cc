#include <gtest/gtest.h>

#include "helpers.h"
#include "wm_helpers.h"
#include "plugin/widgets.h"

using namespace tiny::plugin;

namespace {

int chars(const std::string& text)
{
    return static_cast<int>(text.size()) * 8;
}

int big_chars(const std::string& text)
{
    return static_cast<int>(text.size()) * 20;
}

PopupLayout layout_of(const PopupContent& content, int limit = 1000)
{
    return popup_layout(content, 16, 18, 36, limit, chars, big_chars);
}

} // namespace

TEST(Dimmed, is_halfway_between_the_two_colors)
{
    EXPECT_EQ(dimmed(0xFFFFFF, 0x000000), 0x808080U);
    EXPECT_EQ(dimmed(0x102030, 0x102030), 0x102030U);
    EXPECT_EQ(dimmed(0xFF0000, 0x0000FF), 0x800080U);
}

TEST(ItemIcon, takes_no_room_unless_it_was_asked_for)
{
    ItemIcon icon;
    EXPECT_EQ(icon.width_for(30), 0);
    icon.room = true;
    EXPECT_EQ(ItemIcon::size_for(30), 18);
    EXPECT_GT(icon.width_for(30), ItemIcon::size_for(30));
}

TEST(PopupLayout, an_empty_window_has_only_its_margins)
{
    const PopupLayout layout = layout_of({});
    EXPECT_GT(layout.width, 0);
    EXPECT_GT(layout.height, 0);
    EXPECT_EQ(layout.header_height, 0);
    EXPECT_TRUE(layout.rows.empty());
}

TEST(PopupLayout, the_header_is_as_high_as_its_biggest_part)
{
    PopupContent content;
    content.icon = "weather-clear";
    content.title = "Prague";
    const PopupLayout with_icon = layout_of(content);
    EXPECT_EQ(with_icon.header_height, 36); // two icons
    content.icon.clear();
    content.value = "9";
    EXPECT_EQ(layout_of(content).header_height, 36); // the big value
    content.value.clear();
    EXPECT_EQ(layout_of(content).header_height, 34); // two lines
}

TEST(PopupLayout, a_badge_takes_room_before_the_value)
{
    PopupContent content;
    content.sections.push_back({"", {{"", std::string(40, 'a'), "WPA2"}}});
    const int plain = layout_of(content).width;
    content.sections[0].rows[0].badge = "system-lock-screen";
    EXPECT_EQ(layout_of(content).width, plain + 18 + 6); // the icon and a gap
}

TEST(PopupLayout, the_lines_follow_each_other_in_the_order_of_the_sections)
{
    PopupContent content;
    content.title = "T";
    content.sections.push_back(
            {"Today", {{"", "Wind", "5"}, {"", "Rain", "0"}}});
    content.sections.push_back({"", {{"", "Mon", "1"}}});
    const PopupLayout layout = layout_of(content);
    ASSERT_EQ(layout.rows.size(), 2U);
    ASSERT_EQ(layout.rows[0].size(), 2U);
    EXPECT_GT(layout.headings[0].height, 0);
    EXPECT_EQ(layout.headings[1].height, 0); // no heading, no room
    EXPECT_EQ(layout.rows[0][0].y,
              layout.headings[0].y + layout.headings[0].height);
    EXPECT_EQ(layout.rows[0][1].y,
              layout.rows[0][0].y + layout.rows[0][0].height);
    EXPECT_GT(layout.rows[1][0].y, layout.rows[0][1].y);
    EXPECT_GE(layout.height, layout.rows[1][0].y + layout.rows[1][0].height);
}

TEST(PopupLayout, is_as_wide_as_the_longest_line_but_not_wider_than_the_limit)
{
    PopupContent content;
    content.sections.push_back({"", {{"", std::string(60, 'a'), "1"}}});
    EXPECT_GT(layout_of(content).width, 60 * 8);
    EXPECT_EQ(layout_of(content, 300).width, 300);
    EXPECT_GE(layout_of({}).width, 240); // a short one has a minimum
}

TEST(PluginWidgets, a_label_with_an_icon_is_wider_than_one_without)
{
    const WidgetFont font;
    const ShellFonts shell_fonts;
    ItemIcon icon;
    icon.room = true;
    const PluginLabel plain("12", "12");
    const PluginLabel with("12", "12", icon);
    EXPECT_GT(with.get_width(), plain.get_width());
}

TEST(Mixed, takes_a_part_of_the_second_color)
{
    EXPECT_EQ(mixed(0x000000, 0xFFFFFF, 0), 0x000000U);
    EXPECT_EQ(mixed(0x000000, 0xFFFFFF, 100), 0xFFFFFFU);
    EXPECT_EQ(mixed(0x000000, 0xFF8040, 50), 0x804020U);
}

namespace {

PopupRow action_row(const std::string& label, int action)
{
    PopupRow row;
    row.label = label;
    row.action = action;
    return row;
}

PopupContent two_sections()
{
    PopupContent content;
    PopupRow dim = action_row("dim", 5);
    dim.dim = true;
    PopupRow field;
    field.label = "Password";
    field.entry = true;
    field.action = 7;
    content.sections.push_back(
            {"First", {action_row("a", 0), {"", "plain", "x"}, dim}});
    content.sections.push_back({"Second", {action_row("b", 1), field}});
    return content;
}

} // namespace

TEST(PopupTargets, are_the_rows_with_an_action_or_a_field_that_are_not_dim)
{
    const std::vector<RowRef> targets = popup_targets(two_sections());
    ASSERT_EQ(targets.size(), 3U);
    EXPECT_EQ(targets[0], (RowRef{0, 0}));
    EXPECT_EQ(targets[1], (RowRef{1, 0}));
    EXPECT_EQ(targets[2], (RowRef{1, 1}));
    EXPECT_FALSE(RowRef{}.valid());
    EXPECT_TRUE(targets[0].valid());
    EXPECT_NE(targets[0], targets[1]);
}

TEST(PopupTargets, a_row_is_known_by_its_id_or_its_label)
{
    PopupRow row = action_row("Home", 1);
    EXPECT_EQ(row.key(), "Home");
    row.id = "ssid-1";
    EXPECT_EQ(row.key(), "ssid-1");
}

TEST(PopupRowAt, finds_only_the_rows_that_can_be_used)
{
    const PopupContent content = two_sections();
    const PopupLayout layout = layout_of(content);
    const auto middle = [&](size_t s, size_t r) {
        return layout.rows[s][r].y + layout.rows[s][r].height / 2;
    };
    EXPECT_EQ(popup_row_at(layout, content, middle(0, 0)), (RowRef{0, 0}));
    EXPECT_FALSE(popup_row_at(layout, content, middle(0, 1)).valid()); // plain
    EXPECT_FALSE(popup_row_at(layout, content, middle(0, 2)).valid()); // dim
    EXPECT_EQ(popup_row_at(layout, content, middle(1, 1)), (RowRef{1, 1}));
    EXPECT_FALSE(popup_row_at(layout, content, 0).valid());
    EXPECT_FALSE(popup_row_at(layout, content, 100000).valid());
    EXPECT_FALSE(popup_row_at(layout, content, -5).valid());
}

TEST(PopupScroll, stays_between_the_top_and_the_end_of_the_content)
{
    EXPECT_EQ(clamp_scroll(-10, 500, 200), 0);
    EXPECT_EQ(clamp_scroll(150, 500, 200), 150);
    EXPECT_EQ(clamp_scroll(900, 500, 200), 300);
    EXPECT_EQ(clamp_scroll(50, 100, 200), 0); // all of it is in the view
}

TEST(PopupScroll, moves_the_least_to_show_a_part)
{
    EXPECT_EQ(scroll_to_show(100, 120, 150, 200), 100); // already there
    EXPECT_EQ(scroll_to_show(100, 50, 80, 200), 50);    // above: its top
    EXPECT_EQ(scroll_to_show(100, 250, 320, 200), 120); // below: its bottom
    EXPECT_EQ(scroll_to_show(100, 0, 400, 200), 0);     // too big: its top
}

namespace {

//! A window of a plugin on the display, and what its rows did
class PopupTest : public ::testing::Test {
  protected:
    PopupTest(): anchor("x", "x")
    {
        anchor.realize(root_window(), 100, 10);
        anchor.map();
        popup.realize(root_window());
        popup.set_events();
        popup.set_action([this](int number, const std::string& text) {
            actions.emplace_back(number, text);
            if (on_action) {
                on_action();
            }
        });
    }

    void show(const PopupContent& content)
    {
        popup.open(content, anchor.get_window(), root_window());
    }

    void press(int y, unsigned button = Button1)
    {
        XEvent e = make_event(ButtonPress);
        e.xbutton.window = popup.get_window();
        e.xbutton.button = button;
        e.xbutton.y = y;
        send(popup.get_window(), e);
    }

    void move_to(int y)
    {
        XEvent e = make_event(MotionNotify);
        e.xmotion.window = popup.get_window();
        e.xmotion.y = y;
        send(popup.get_window(), e);
    }

    //! Inside of a row, as the pointer sees it
    int at(size_t section, size_t row) const
    {
        const PopupLayout::Line& line = popup.placed().rows[section][row];
        return line.y + line.height / 2 - popup.scrolled();
    }

    void key(KeySym sym) { type_key(popup.get_window(), sym); }

    WidgetFont font;
    ShellFonts shell_fonts;
    PluginLabel anchor;
    PluginPopup popup{nullptr};
    std::vector<std::pair<int, std::string>> actions;
    std::function<void()> on_action;
};

PopupContent many_rows(int count)
{
    PopupContent content;
    PopupSection section;
    for (int i = 0; i < count; ++i) {
        section.rows.push_back(action_row("row " + std::to_string(i), i));
    }
    content.sections.push_back(section);
    return content;
}

} // namespace

TEST_F(PopupTest, the_pointer_lights_a_row_that_can_be_used)
{
    show(two_sections());
    EXPECT_EQ(popup.selected_row(), (RowRef{1, 1})); // the field has the keys
    move_to(at(0, 0));
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 0}));
    move_to(at(0, 1)); // a row that only shows something
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 0}));
    move_to(at(0, 2)); // a dim one
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 0}));
    EXPECT_TRUE(actions.empty()); // moving does nothing
}

TEST_F(PopupTest, a_click_calls_the_action_of_the_row)
{
    show(two_sections());
    press(at(1, 0));
    ASSERT_EQ(actions.size(), 1U);
    EXPECT_EQ(actions[0], std::make_pair(1, std::string()));
    press(at(0, 1)); // nothing to do there
    press(at(0, 2));
    press(at(0, 0), Button3); // not the main button
    EXPECT_EQ(actions.size(), 1U);
}

TEST_F(PopupTest, the_arrows_and_return_work_without_a_pointer)
{
    show(many_rows(3));
    EXPECT_FALSE(popup.selected_row().valid());
    key(XK_Down);
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 0}));
    key(XK_Tab);
    key(XK_Down);
    key(XK_Down); // the end: stays
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 2}));
    key(XK_Up);
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 1}));
    key(XK_Return);
    ASSERT_EQ(actions.size(), 1U);
    EXPECT_EQ(actions[0].first, 1);
    key(XK_Escape);
    EXPECT_FALSE(popup.get_maped());
}

TEST_F(PopupTest, up_from_nothing_goes_to_the_last_row)
{
    show(many_rows(3));
    key(XK_Up);
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 2}));
}

TEST_F(PopupTest, keys_do_nothing_in_a_window_without_rows_to_use)
{
    show({});
    key(XK_Down);
    key(XK_Return);
    EXPECT_FALSE(popup.selected_row().valid());
    EXPECT_TRUE(actions.empty());
}

TEST_F(PopupTest, a_field_takes_the_typed_text_and_gives_it_with_return)
{
    show(two_sections());
    type_text(popup.get_window(), "abc");
    EXPECT_EQ(popup.typed(), "abc");
    key(XK_BackSpace);
    key(XK_Left);
    key(XK_Delete); // the "b" after the cursor
    type_text(popup.get_window(), "x");
    key(XK_Home);
    key(XK_End);
    key(XK_Right);
    EXPECT_EQ(popup.typed(), "ax");
    key(XK_Return);
    ASSERT_EQ(actions.size(), 1U);
    EXPECT_EQ(actions[0], std::make_pair(7, std::string("ax")));
    EXPECT_EQ(popup.typed(), ""); // given away, not kept
}

TEST_F(PopupTest, a_field_ignores_commands_and_keys_outside_of_it)
{
    show(two_sections());
    type_key(popup.get_window(), XK_a, ControlMask);
    EXPECT_EQ(popup.typed(), "");
    key(XK_Up); // away from the field
    type_text(popup.get_window(), "z");
    EXPECT_EQ(popup.typed(), "");
    key(XK_Left);
    key(XK_BackSpace);
    EXPECT_EQ(popup.typed(), "");
}

TEST_F(PopupTest, a_field_does_not_take_a_text_that_is_too_long)
{
    show(two_sections());
    type_text(popup.get_window(), std::string(300, 'a'));
    EXPECT_EQ(popup.typed().size(), 256U);
}

TEST_F(PopupTest,
       the_text_stays_when_the_window_is_shown_again_and_not_after_it_closes)
{
    show(two_sections());
    type_text(popup.get_window(), "keep");
    show(two_sections()); // a late answer
    EXPECT_EQ(popup.typed(), "keep");
    EXPECT_EQ(popup.selected_row(), (RowRef{1, 1}));

    PopupContent other = two_sections();
    other.sections[1].rows[1].label = "Another";
    show(other); // another field
    EXPECT_EQ(popup.typed(), "");

    type_text(popup.get_window(), "gone");
    popup.unmap();
    show(two_sections());
    EXPECT_EQ(popup.typed(), "");
}

TEST_F(PopupTest, a_late_answer_keeps_the_lit_row_by_its_id)
{
    PopupContent content = many_rows(3);
    content.sections[0].rows[1].id = "two";
    show(content);
    move_to(at(0, 1));
    ASSERT_EQ(popup.selected_row(), (RowRef{0, 1}));
    // the list changed: the same network is the first one now
    PopupContent shuffled = many_rows(3);
    std::swap(shuffled.sections[0].rows[0], shuffled.sections[0].rows[1]);
    shuffled.sections[0].rows[0].id = "two";
    show(shuffled);
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 0}));
    show(many_rows(1)); // gone
    EXPECT_FALSE(popup.selected_row().valid());
}

TEST_F(PopupTest, a_field_that_appears_in_an_open_window_gets_the_keys)
{
    show(many_rows(3));
    move_to(at(0, 1));
    ASSERT_EQ(popup.selected_row(), (RowRef{0, 1}));
    show(two_sections()); // the user clicked a network that needs a password
    EXPECT_EQ(popup.selected_row(), (RowRef{1, 1}));
    type_text(popup.get_window(), "pw");
    EXPECT_EQ(popup.typed(), "pw");
}

TEST_F(PopupTest, an_action_may_show_the_window_again)
{
    show(many_rows(3));
    on_action = [this] { show(two_sections()); };
    press(at(0, 1));
    ASSERT_EQ(actions.size(), 1U);
    EXPECT_EQ(popup.placed().rows.size(), 2U);
}

TEST_F(PopupTest, a_high_window_is_scrolled_by_the_wheel_and_the_keys)
{
    show(many_rows(200));
    ASSERT_LT(popup.view(), popup.placed().height);
    EXPECT_EQ(popup.scrolled(), 0);
    press(10, Button4); // up at the top: stays
    EXPECT_EQ(popup.scrolled(), 0);
    press(10, Button5);
    const int one = popup.scrolled();
    EXPECT_GT(one, 0);
    press(10, Button5);
    EXPECT_GT(popup.scrolled(), one);
    press(10, Button4);
    EXPECT_EQ(popup.scrolled(), one);
    key(XK_Next);
    EXPECT_GT(popup.scrolled(), one);
    key(XK_Prior);
    EXPECT_EQ(popup.scrolled(), one);
    for (int i = 0; i < 100; ++i) {
        press(10, Button5);
    }
    EXPECT_EQ(popup.scrolled(), popup.placed().height - popup.view());
}

TEST_F(PopupTest, the_keys_scroll_to_the_row_that_is_lit_and_a_click_hits_it)
{
    show(many_rows(200));
    for (int i = 0; i < 120; ++i) {
        key(XK_Down);
    }
    EXPECT_EQ(popup.selected_row(), (RowRef{0, 119}));
    EXPECT_GT(popup.scrolled(), 0);
    const PopupLayout::Line& line = popup.placed().rows[0][119];
    EXPECT_GE(line.y - popup.scrolled(), 0);
    EXPECT_LE(line.y + line.height - popup.scrolled(), popup.view());
    press(at(0, 119)); // the pointer coordinates are of the window
    ASSERT_FALSE(actions.empty());
    EXPECT_EQ(actions.back().first, 119);
    for (int i = 0; i < 100; ++i) {
        key(XK_Down); // to the end: the whole end is shown
    }
    EXPECT_EQ(popup.scrolled(), popup.placed().height - popup.view());
    key(XK_Up);
    for (int i = 0; i < 300; ++i) {
        key(XK_Up);
    }
    EXPECT_EQ(popup.scrolled(), 0); // the first row shows what is above it
}

TEST_F(PopupTest, a_window_that_fits_is_not_scrolled)
{
    show(many_rows(2));
    EXPECT_EQ(popup.view(), popup.placed().height);
    press(10, Button5);
    EXPECT_EQ(popup.scrolled(), 0);
}
