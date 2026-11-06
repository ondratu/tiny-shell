#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "wm/switcher_model.h"

using tiny::wm::SwitcherModel;
using Level = SwitcherModel::Level;
using Keys = std::vector<std::string>;
using Indexes = std::vector<size_t>;

namespace {

// The windows, the latest first: a, b, a, c, b -> the applications a, b, c
const Keys keys = {"a", "b", "a", "c", "b"};

} // namespace

TEST(SwitcherModelTest, every_application_is_there_once_in_the_order_of_use)
{
    const SwitcherModel model(keys);

    ASSERT_EQ(model.apps().size(), 3u);
    EXPECT_EQ(model.apps()[0].key, "a");
    EXPECT_EQ(model.apps()[0].tasks, (Indexes{0, 2}));
    EXPECT_EQ(model.apps()[1].tasks, (Indexes{1, 4}));
    EXPECT_EQ(model.apps()[2].tasks, (Indexes{3}));
}

TEST(SwitcherModelTest, a_window_without_a_key_is_an_application_of_its_own)
{
    const SwitcherModel model({"", "", "a"});

    EXPECT_EQ(model.apps().size(), 3u);
}

TEST(SwitcherModelTest, alt_tab_starts_on_the_application_used_before)
{
    SwitcherModel model(keys);

    model.start_apps(false);

    EXPECT_EQ(model.get_level(), Level::Apps);
    EXPECT_EQ(model.get_app(), 1u);
    EXPECT_EQ(model.result(), 1u); // its window used last
}

TEST(SwitcherModelTest, alt_shift_tab_starts_on_the_last_application)
{
    SwitcherModel model(keys);

    model.start_apps(true);

    EXPECT_EQ(model.get_app(), 2u);
    EXPECT_EQ(model.result(), 3u);
}

TEST(SwitcherModelTest, with_one_application_alt_tab_switches_nothing)
{
    SwitcherModel model({"a", "a"});

    model.start_apps(false);

    EXPECT_EQ(model.get_app(), 0u);
    EXPECT_EQ(model.result(), 0u);
}

TEST(SwitcherModelTest, tab_goes_through_the_applications_and_wraps_around)
{
    SwitcherModel model(keys);
    model.start_apps(false);

    model.next_app(false);
    EXPECT_EQ(model.get_app(), 2u);
    model.next_app(false);
    EXPECT_EQ(model.get_app(), 0u);
    EXPECT_EQ(model.result(), 0u); // back on the current one
    model.next_app(true);
    EXPECT_EQ(model.get_app(), 2u);
}

TEST(SwitcherModelTest, the_arrows_move_on_the_applications_like_tab)
{
    SwitcherModel model(keys);
    model.start_apps(false);

    model.move(false);
    EXPECT_EQ(model.get_app(), 2u);
    model.move(true);
    model.move(true);
    EXPECT_EQ(model.get_app(), 0u);
}

TEST(SwitcherModelTest, down_shows_the_windows_and_stands_on_the_first_one)
{
    SwitcherModel model(keys);
    model.start_apps(false);

    model.down();

    EXPECT_EQ(model.get_level(), Level::Windows);
    EXPECT_EQ(model.get_window(), 0u);
    EXPECT_EQ(model.app_windows(), (Indexes{1, 4}));
    EXPECT_EQ(model.result(), 1u);
}

TEST(SwitcherModelTest, the_arrows_move_between_the_windows_and_wrap_around)
{
    SwitcherModel model(keys);
    model.start_apps(false);
    model.down();

    model.move(false);
    EXPECT_EQ(model.get_window(), 1u);
    EXPECT_EQ(model.result(), 4u);
    model.move(false);
    EXPECT_EQ(model.get_window(), 0u);
    model.move(true);
    EXPECT_EQ(model.get_window(), 1u);
    EXPECT_EQ(model.get_app(), 1u); // the application stays
}

TEST(SwitcherModelTest, down_on_the_windows_does_nothing)
{
    SwitcherModel model(keys);
    model.start_apps(false);
    model.down();
    model.move(false);

    model.down();

    EXPECT_EQ(model.get_window(), 1u);
}

TEST(SwitcherModelTest, up_goes_back_to_the_application)
{
    SwitcherModel model(keys);
    model.start_apps(false);
    model.down();
    model.move(false);

    model.up();

    EXPECT_EQ(model.get_level(), Level::Apps);
    EXPECT_EQ(model.get_app(), 1u);
    EXPECT_EQ(model.result(), 1u); // the first window of the application
    model.up();                    // nothing more to go up to
    EXPECT_EQ(model.get_level(), Level::Apps);
}

TEST(SwitcherModelTest, tab_on_the_windows_shows_the_windows_of_the_next_app)
{
    SwitcherModel model(keys);
    model.start_apps(false);
    model.down();
    model.move(false);

    model.next_app(false);

    EXPECT_EQ(model.get_level(), Level::Windows);
    EXPECT_EQ(model.get_app(), 2u);
    EXPECT_EQ(model.get_window(), 0u);
    EXPECT_EQ(model.result(), 3u);
}

TEST(SwitcherModelTest, alt_semicolon_starts_on_the_window_used_before)
{
    SwitcherModel model(keys);

    model.start_windows(false);

    EXPECT_EQ(model.get_level(), Level::Windows);
    EXPECT_EQ(model.get_app(), 0u);
    EXPECT_EQ(model.get_window(), 1u);
    EXPECT_EQ(model.result(), 2u);
}

TEST(SwitcherModelTest, alt_shift_semicolon_starts_on_the_oldest_window)
{
    SwitcherModel model({"a", "a", "a", "b"});

    model.start_windows(true);

    EXPECT_EQ(model.get_window(), 2u);
    EXPECT_EQ(model.result(), 2u);
}

TEST(SwitcherModelTest, semicolon_with_one_window_stays_on_the_current_one)
{
    SwitcherModel model({"a", "b"});

    model.start_windows(false);

    EXPECT_EQ(model.get_window(), 0u);
    EXPECT_EQ(model.result(), 0u);
}

TEST(SwitcherModelTest, semicolon_walks_the_windows_of_the_application)
{
    SwitcherModel model({"a", "b", "a", "a"});
    model.start_windows(false);
    ASSERT_EQ(model.get_window(), 1u);

    model.next_window(false);
    EXPECT_EQ(model.get_window(), 2u);
    model.next_window(false);
    EXPECT_EQ(model.get_window(), 0u);
    model.next_window(true);
    EXPECT_EQ(model.get_window(), 2u);
}

TEST(SwitcherModelTest, semicolon_on_an_application_opens_its_windows)
{
    SwitcherModel model(keys);
    model.start_apps(false);

    model.next_window(false);

    EXPECT_EQ(model.get_level(), Level::Windows);
    EXPECT_EQ(model.get_app(), 1u);
    EXPECT_EQ(model.get_window(), 1u); // the one after the first
    EXPECT_EQ(model.result(), 4u);
}

TEST(SwitcherModelTest, there_is_nothing_to_choose_without_windows)
{
    SwitcherModel model({});

    model.start_apps(false);
    model.next_app(true);
    model.move(false);
    model.down();
    model.up();
    model.start_windows(false);
    model.next_window(false);

    EXPECT_TRUE(model.apps().empty());
    EXPECT_TRUE(model.app_windows().empty());
    EXPECT_EQ(model.result(), std::nullopt);
}

TEST(SwitcherModelTest, move_on_a_level_of_windows_without_any_does_nothing)
{
    SwitcherModel model({});

    model.move(false); // the level of applications, no application

    EXPECT_EQ(model.get_app(), 0u);
}
