#include "wm_helpers.h"
#include "wm/shell_theme.h"

#include "wm/keyboard_indicator.h"

using tiny::wm::KeyboardGroups;
using tiny::wm::KeyboardIndicator;
using tiny::wm::KeyboardMenu;
using tiny::wm::make_keyboard_groups;
using tiny::wm::XkbGroups;
using Strings = std::vector<std::string>;

// ---- The groups ----

TEST(KeyboardGroupsTest, the_codes_are_the_layouts_and_the_names_come_with_them)
{
    const KeyboardGroups groups =
            make_keyboard_groups("cz,us", {"Czech", "English (US)"}, 1);

    EXPECT_EQ(groups.codes, (Strings{"cz", "us"}));
    EXPECT_EQ(groups.names, (Strings{"Czech", "English (US)"}));
    EXPECT_EQ(groups.current, 1u);
    EXPECT_EQ(groups.code(), "us");
    EXPECT_EQ(groups.size(), 2u);
}

TEST(KeyboardGroupsTest, a_group_without_a_name_is_named_by_its_code)
{
    const KeyboardGroups groups = make_keyboard_groups("cz,us", {"Czech"}, 0);

    EXPECT_EQ(groups.names, (Strings{"Czech", "us"}));
}

TEST(KeyboardGroupsTest, without_layouts_the_names_make_the_codes)
{
    const KeyboardGroups groups =
            make_keyboard_groups("", {"Czech", "English (US)", ""}, 0);

    EXPECT_EQ(groups.codes, (Strings{"cz", "en"}));
    EXPECT_EQ(groups.names, (Strings{"Czech", "English (US)"}));
}

TEST(KeyboardGroupsTest, a_group_that_does_not_exist_is_the_first_one)
{
    EXPECT_EQ(make_keyboard_groups("cz,us", {}, 7).current, 0u);
}

TEST(KeyboardGroupsTest, there_are_no_more_than_four_and_none_without_layouts)
{
    EXPECT_EQ(make_keyboard_groups("a,b,c,d,e", {}, 0).size(), 4u);
    const KeyboardGroups none = make_keyboard_groups("", {}, 0);
    EXPECT_EQ(none.size(), 0u);
    EXPECT_EQ(none.code(), "");
}

// ---- The keyboard of the server ----

TEST(XkbGroupsTest, the_groups_of_the_server_are_read_without_changing_them)
{
    const XkbGroups xkb(tiny::get_display());
    if (!xkb.available()) {
        GTEST_SKIP() << "no XKB in this X server";
    }
    xkb.select_events();

    const KeyboardGroups groups = xkb.read();

    EXPECT_EQ(groups.codes.size(), groups.names.size());
    EXPECT_TRUE(groups.size() == 0 || groups.current < groups.size());
}

TEST(XkbGroupsTest, only_the_events_of_xkb_are_its_own)
{
    const XkbGroups xkb(tiny::get_display());

    EXPECT_FALSE(xkb.is_event(make_event(KeyPress)));
    EXPECT_FALSE(xkb.is_event(make_event(MotionNotify)));
}

// ---- The menu ----

TEST(KeyboardMenuTest, has_an_item_for_each_layout_and_marks_the_one_in_use)
{
    const KeyboardGroups groups =
            make_keyboard_groups("cz,us", {"Czech", "English (US)"}, 1);
    KeyboardMenu menu(groups);

    menu.realize(root_window());

    const int item = tiny::wm::shell_theme.popup_item_height;
    EXPECT_EQ(attributes_of(menu).height, 2 * item);
    EXPECT_EQ(children_of(menu.get_window()).size(), 2u);
    const ::Window second = child_at(menu.get_window(), 0, item);
    EXPECT_NE(second, None);
}

TEST(KeyboardMenuTest, a_click_on_an_item_hides_the_menu_and_tells_which)
{
    const WidgetFont font;
    const ShellFonts shell_fonts;
    KeyboardMenu menu(make_keyboard_groups("cz,us", {}, 0));
    std::vector<size_t> chosen;
    menu.on_choose = [&](size_t group) { chosen.push_back(group); };
    menu.realize(root_window());

    const ::Window second = child_at(menu.get_window(), 0,
                                     tiny::wm::shell_theme.popup_item_height);
    send(second, make_event(EnterNotify)); // a click counts only on hover
    send(second, make_event(ButtonRelease));

    EXPECT_EQ(chosen, (std::vector<size_t>{1}));
    EXPECT_FALSE(menu.get_maped());
}

// ---- The indicator ----

TEST(KeyboardIndicatorTest, shows_the_code_of_the_layout_in_use_cut_to_three)
{
    const WidgetFont font;
    const ShellFonts shell_fonts;
    KeyboardIndicator indicator;

    indicator.set_groups(make_keyboard_groups("cz,us", {}, 1));
    EXPECT_EQ(indicator.get_text(), "us");
    indicator.set_groups(make_keyboard_groups("latam", {}, 0));
    EXPECT_EQ(indicator.get_text(), "lat");
    indicator.set_groups(make_keyboard_groups("", {}, 0));
    EXPECT_EQ(indicator.get_text(), "");
}

TEST(KeyboardIndicatorTest, has_the_height_of_the_panel_and_a_fixed_width)
{
    const WidgetFont font;
    const ShellFonts shell_fonts;
    KeyboardIndicator indicator;
    const int width = indicator.get_width();

    indicator.set_groups(make_keyboard_groups("cz,us", {}, 0));
    indicator.set_groups(make_keyboard_groups("us", {}, 0));

    EXPECT_EQ(indicator.get_height(), tiny::wm::shell_theme.panel);
    EXPECT_GT(width, 0);
    EXPECT_EQ(indicator.get_width(), width);
}

TEST(KeyboardIndicatorTest, a_click_with_one_layout_opens_nothing)
{
    const WidgetFont font;
    const ShellFonts shell_fonts;
    KeyboardIndicator indicator;
    indicator.realize(root_window(), 0, 0);
    indicator.set_events();
    size_t chosen = 99;
    indicator.on_choose = [&](size_t group) { chosen = group; };
    const auto click = [&] {
        send(indicator, EnterNotify);
        XEvent release = make_event(ButtonRelease);
        release.xbutton.window = indicator.get_window();
        send(indicator.get_window(), release);
    };

    indicator.set_groups(make_keyboard_groups("us", {}, 0));
    click(); // one layout: no menu, nothing to choose
    EXPECT_EQ(chosen, 99u);
}
