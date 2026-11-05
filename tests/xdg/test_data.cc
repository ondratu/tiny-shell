#include <gtest/gtest.h>

#include <filesystem>

#include "wm/config.h"
#include "xdg/desktop_entry.h"

// The files of data/ (what `make install` puts into share/) are where the
// code looks for them; the tests run in the root of the repository.

namespace fs = std::filesystem;

namespace {

const fs::path data = "data";

} // namespace

TEST(DataTest, the_desktop_files_are_applications_with_an_icon_that_is_there)
{
    int found = 0;
    for (const auto& file : fs::directory_iterator(data / "applications")) {
        const auto entry =
                tiny::xdg::read_desktop_entry(file.path().string(), "");
        ASSERT_TRUE(entry) << file.path();
        ++found;
        EXPECT_EQ(entry->type, "Application") << file.path();
        EXPECT_FALSE(entry->name.empty()) << file.path();
        EXPECT_EQ(entry->exec, file.path().stem().string()) << file.path();
        EXPECT_FALSE(entry->startup_wm_class.empty()) << file.path();
        EXPECT_TRUE(fs::exists(data / "icons/hicolor/scalable/apps" /
                               (entry->icon + ".svg")))
                << file.path();
    }
    EXPECT_EQ(found, 3);
}

TEST(DataTest, the_icons_that_the_shell_draws_are_in_both_colors)
{
    for (const char* name :
         {"start-here", "window-close", "window-minimize", "window-maximize",
          "window-restore", "view-app-grid", "application-x-executable"}) {
        for (const char* color : {"white", "black"}) {
            EXPECT_TRUE(fs::exists(data / "tiny-shell/icons" / color /
                                   (std::string(name) + "-symbolic.svg")))
                    << color << "/" << name;
        }
    }
}

TEST(DataTest, the_default_wallpapers_are_there_for_both_styles)
{
    const std::vector<std::string> dirs = {data.string()};

    EXPECT_FALSE(
            tiny::wm::default_wallpaper(tiny::Variant::Light, dirs).empty());
    EXPECT_FALSE(
            tiny::wm::default_wallpaper(tiny::Variant::Dark, dirs).empty());
}
