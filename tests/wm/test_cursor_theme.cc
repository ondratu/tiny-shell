#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <string>
#include <vector>

#ifdef TINY_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#include "display.h"
#include "wm/cursor_theme.h"

using tiny::wm::cursor_resources;
using tiny::wm::ResourceChange;
using tiny::wm::valid_cursor_theme;

TEST(CursorThemeTest, a_name_of_a_theme_is_a_name_of_a_directory)
{
    EXPECT_TRUE(valid_cursor_theme("Adwaita"));
    EXPECT_TRUE(valid_cursor_theme("Bibata-Modern Classic"));
    EXPECT_FALSE(valid_cursor_theme(""));
    EXPECT_FALSE(valid_cursor_theme("../Adwaita"));
    EXPECT_FALSE(valid_cursor_theme("a/b"));
    EXPECT_FALSE(valid_cursor_theme(".hidden"));
    EXPECT_FALSE(valid_cursor_theme("two\nlines"));
    EXPECT_FALSE(valid_cursor_theme(std::string(200, 'a')));
}

TEST(CursorThemeTest, the_resources_have_the_theme_and_the_size)
{
    const auto changes = cursor_resources("Adwaita", 32);

    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0], (ResourceChange{"Xcursor.theme", "Adwaita"}));
    EXPECT_EQ(changes[1], (ResourceChange{"Xcursor.size", "32"}));
}

TEST(CursorThemeTest, what_is_not_set_removes_its_resource)
{
    const auto changes = cursor_resources("", 0);

    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0].value, "");
    EXPECT_EQ(changes[1].value, "");
}

TEST(CursorThemeTest, no_resources_without_a_setting)
{
    EXPECT_TRUE(cursor_resources(tiny::wm::Config{}).empty());

    tiny::wm::Config config;
    config.cursor_size = 48;
    const auto changes = cursor_resources(config);
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[1].value, "48");
}

TEST(CursorThemeTest, the_sizes_of_the_form_are_right_and_start_with_the_system)
{
    const auto sizes = tiny::wm::cursor_size_choices();

    ASSERT_GT(sizes.size(), 1u);
    EXPECT_STREQ(sizes[0].value, "");
    for (const auto& size : sizes) {
        EXPECT_TRUE(tiny::wm::valid_cursor_size(size.value));
    }
    EXPECT_FALSE(tiny::wm::valid_cursor_size("0"));
    EXPECT_FALSE(tiny::wm::valid_cursor_size("257"));
    EXPECT_FALSE(tiny::wm::valid_cursor_size("12px"));
}

TEST(CursorThemeTest, the_installed_themes_are_the_directories_with_cursors)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() /
                          ("tiny-cursors-" + std::to_string(getpid()));
    fs::create_directories(root / "share/icons/Alpha/cursors");
    fs::create_directories(root / "share/icons/NoCursors/16x16");
    fs::create_directories(root / "home/.icons/Beta/cursors");
    fs::create_directories(root / "share/icons/Alpha-copy");
    fs::create_directories(root / "home/.icons/Alpha/cursors"); // twice

    const auto names = tiny::wm::installed_cursor_themes(
            {(root / "share").string(), (root / "nothing").string()},
            (root / "home").c_str());

    EXPECT_EQ(names, (std::vector<std::string>{"Alpha", "Beta"}));
    fs::remove_all(root);
}

#ifdef TINY_XCURSOR
TEST(CursorThemeTest, the_display_takes_the_theme_and_the_size)
{
    ::Display* display = tiny::get_display();

    tiny::wm::apply_cursor_theme(display, "tiny-test-theme", 40);

    EXPECT_STREQ(XcursorGetTheme(display), "tiny-test-theme");
    EXPECT_EQ(XcursorGetDefaultSize(display), 40);
    EXPECT_TRUE(tiny::wm::has_cursor_themes());
    XcursorSetTheme(display, nullptr); // the next tests do not need it
}

TEST(CursorThemeTest, nothing_set_gives_back_what_the_system_says)
{
    ::Display* display = tiny::get_display();
    tiny::wm::apply_cursor_theme(display, "tiny-test-theme", 40);

    tiny::wm::apply_cursor_theme(display, "", 0);

    EXPECT_STRNE(XcursorGetTheme(display), "tiny-test-theme");
    EXPECT_NE(XcursorGetDefaultSize(display), 40);
}
#endif
