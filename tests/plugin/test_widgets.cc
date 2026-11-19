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

TEST(PopupLayout, the_lines_follow_each_other_in_the_order_of_the_sections)
{
    PopupContent content;
    content.title = "T";
    content.sections.push_back({"Today", {{"", "Wind", "5"}, {"", "Rain", "0"}}});
    content.sections.push_back({"", {{"", "Mon", "1"}}});
    const PopupLayout layout = layout_of(content);
    ASSERT_EQ(layout.rows.size(), 2U);
    ASSERT_EQ(layout.rows[0].size(), 2U);
    EXPECT_GT(layout.headings[0].height, 0);
    EXPECT_EQ(layout.headings[1].height, 0); // no heading, no room
    EXPECT_EQ(layout.rows[0][0].y, layout.headings[0].y + layout.headings[0].height);
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
