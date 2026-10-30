#include <gtest/gtest.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "icon_theme.h"

namespace fs = std::filesystem;

namespace {

class IconThemeTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-icons" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "icons");
        fs::create_directories(root / "pixmaps");
    }

    void TearDown() override { fs::remove_all(root); }

    void touch(const std::string& file) const
    {
        const auto path = root / file;
        fs::create_directories(path.parent_path());
        std::ofstream(path) << "x";
    }

    void index(const std::string& theme, const std::string& body) const
    {
        touch("icons/" + theme + "/index.theme");
        std::ofstream(root / "icons" / theme / "index.theme") << body;
    }

    std::string path(const std::string& file) const
    {
        return (root / file).string();
    }

    tiny::IconTheme make(const std::string& theme = "hicolor") const
    {
        return tiny::IconTheme({path("icons")}, {path("pixmaps")}, theme);
    }

    fs::path root;
};

const char* const HICOLOR =
        "[Icon Theme]\nDirectories=16x16/apps,48x48/apps,scalable/apps,"
        "64x64/apps\n"
        "[16x16/apps]\nSize=16\nType=Fixed\n"
        "[48x48/apps]\nSize=48\nType=Threshold\nThreshold=4\n"
        "[scalable/apps]\nSize=128\nMinSize=64\nMaxSize=256\nType=Scalable\n"
        "[64x64/apps]\nSize=64\nType=Fixed\nScale=2\n";

} // namespace

TEST(IconDirs, user_then_data_dirs)
{
    const auto dirs = tiny::icon_dirs("/h", "/a:/b", "/home/u");
    ASSERT_EQ(dirs.size(), 4u);
    EXPECT_EQ(dirs[0], "/home/u/.icons");
    EXPECT_EQ(dirs[1], "/h/icons");
    EXPECT_EQ(dirs[2], "/a/icons");
    EXPECT_EQ(dirs[3], "/b/icons");
}

TEST(IconDirs, defaults_and_no_home)
{
    const auto dirs = tiny::icon_dirs(nullptr, nullptr, nullptr);
    ASSERT_EQ(dirs.size(), 2u);
    EXPECT_EQ(dirs[0], "/usr/local/share/icons");
    EXPECT_EQ(dirs[1], "/usr/share/icons");
}

TEST(IconDirs, pixmaps)
{
    const auto dirs = tiny::pixmap_dirs(nullptr, "/a:/", "/home/u");
    ASSERT_EQ(dirs.size(), 3u);
    EXPECT_EQ(dirs[0], "/home/u/.local/share/pixmaps");
    EXPECT_EQ(dirs[1], "/a/pixmaps");
    EXPECT_EQ(dirs[2], "/pixmaps");
}

TEST_F(IconThemeTest, closest_size_wins)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/16x16/apps/app.png");
    touch("icons/hicolor/48x48/apps/app.png");
    const auto theme = make();

    EXPECT_EQ(theme.find("app", 16), path("icons/hicolor/16x16/apps/app.png"));
    EXPECT_EQ(theme.find("app", 20), path("icons/hicolor/16x16/apps/app.png"));
    EXPECT_EQ(theme.find("app", 48), path("icons/hicolor/48x48/apps/app.png"));
    EXPECT_EQ(theme.find("app", 51), path("icons/hicolor/48x48/apps/app.png"));
    EXPECT_EQ(theme.find("app", 200), path("icons/hicolor/48x48/apps/app.png"));
}

TEST_F(IconThemeTest, scalable_directory_with_png_and_range)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/16x16/apps/app.png");
    touch("icons/hicolor/scalable/apps/app.png");
    const auto theme = make();

    EXPECT_EQ(theme.find("app", 100),
              path("icons/hicolor/scalable/apps/app.png"));
    EXPECT_EQ(theme.find("app", 20), path("icons/hicolor/16x16/apps/app.png"));
}

TEST_F(IconThemeTest, png_beats_xpm_at_one_size)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/16x16/apps/both.xpm");
    touch("icons/hicolor/16x16/apps/both.png");
    EXPECT_EQ(make().find("both", 16),
              path("icons/hicolor/16x16/apps/both.png"));
}

TEST_F(IconThemeTest, a_bitmap_of_the_right_size_beats_svg)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/scalable/apps/app.svg");
    touch("icons/hicolor/16x16/apps/app.png");
    touch("icons/hicolor/48x48/apps/app.png");
    const auto theme = make();

    EXPECT_EQ(theme.find("app", 16), path("icons/hicolor/16x16/apps/app.png"));
    EXPECT_EQ(theme.find("app", 50), path("icons/hicolor/48x48/apps/app.png"));
}

TEST_F(IconThemeTest, svg_beats_a_bitmap_of_another_size)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/scalable/apps/app.svg");
    touch("icons/hicolor/16x16/apps/app.png");
    touch("icons/hicolor/48x48/apps/app.png");
    const auto theme = make();

    EXPECT_EQ(theme.find("app", 24),
              path("icons/hicolor/scalable/apps/app.svg"));
    EXPECT_EQ(theme.find("app", 512),
              path("icons/hicolor/scalable/apps/app.svg"));
}

TEST_F(IconThemeTest, svg_alone_serves_every_size)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/scalable/apps/vector.svg");
    const auto theme = make();

    EXPECT_EQ(theme.find("vector", 16),
              path("icons/hicolor/scalable/apps/vector.svg"));
    EXPECT_EQ(theme.find("vector.svg", 300),
              path("icons/hicolor/scalable/apps/vector.svg"));
}

TEST_F(IconThemeTest, closest_bitmap_when_there_is_no_svg)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/16x16/apps/app.png");
    EXPECT_EQ(make().find("app", 24), path("icons/hicolor/16x16/apps/app.png"));
}

TEST_F(IconThemeTest, scaled_directories_are_skipped)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/64x64/apps/big.png");
    EXPECT_EQ(make().find("big", 64), "");
}

TEST_F(IconThemeTest, inherited_theme_then_hicolor)
{
    index("hicolor", HICOLOR);
    index("child", "[Icon Theme]\nInherits=parent\nDirectories=16x16/apps\n"
                   "[16x16/apps]\nSize=16\nType=Fixed\n");
    index("parent", "[Icon Theme]\nDirectories=16x16/apps\n"
                    "[16x16/apps]\nSize=16\nType=Fixed\n");
    touch("icons/child/16x16/apps/own.png");
    touch("icons/parent/16x16/apps/own.png");
    touch("icons/parent/16x16/apps/inherited.png");
    touch("icons/hicolor/16x16/apps/fallback.png");
    const auto theme = make("child");

    EXPECT_EQ(theme.find("own", 16), path("icons/child/16x16/apps/own.png"));
    EXPECT_EQ(theme.find("inherited", 16),
              path("icons/parent/16x16/apps/inherited.png"));
    EXPECT_EQ(theme.find("fallback", 16),
              path("icons/hicolor/16x16/apps/fallback.png"));
}

TEST_F(IconThemeTest, a_theme_wins_over_a_closer_size_in_a_later_one)
{
    index("hicolor", HICOLOR);
    index("mine", "[Icon Theme]\nDirectories=16x16/apps\n"
                  "[16x16/apps]\nSize=16\nType=Fixed\n");
    touch("icons/mine/16x16/apps/app.png");
    touch("icons/hicolor/48x48/apps/app.png");
    EXPECT_EQ(make("mine").find("app", 48),
              path("icons/mine/16x16/apps/app.png"));
}

TEST_F(IconThemeTest, parents_are_searched_depth_first)
{
    // a inherits b and c, b inherits d: a, b, d, c
    const std::string body =
            "[Icon Theme]\nDirectories=x\n[x]\nSize=16\nType=Fixed\n";
    index("hicolor", HICOLOR);
    index("a", "[Icon Theme]\nInherits=b,c\nDirectories=x\n[x]\nSize=16\n");
    index("b", "[Icon Theme]\nInherits=d\nDirectories=x\n[x]\nSize=16\n");
    index("c", body);
    index("d", body);
    touch("icons/c/x/both.png");
    touch("icons/d/x/both.png");
    touch("icons/c/x/only-c.png");
    const auto theme = make("a");

    EXPECT_EQ(theme.find("both", 16), path("icons/d/x/both.png"));
    EXPECT_EQ(theme.find("only-c", 16), path("icons/c/x/only-c.png"));
}

TEST_F(IconThemeTest, a_loop_of_inherits_ends)
{
    index("hicolor", HICOLOR);
    index("a", "[Icon Theme]\nInherits=b\nDirectories=x\n[x]\nSize=16\n");
    index("b",
          "[Icon Theme]\nInherits=a,hicolor\nDirectories=x\n[x]\nSize=16\n");
    touch("icons/b/x/app.png");
    EXPECT_EQ(make("a").find("app", 16), path("icons/b/x/app.png"));
}

TEST_F(IconThemeTest, pixmaps_are_the_last_resort)
{
    index("hicolor", HICOLOR);
    touch("pixmaps/old.xpm");
    touch("pixmaps/vector.svg");
    touch("pixmaps/both.png");
    touch("icons/hicolor/16x16/apps/both.png");
    const auto theme = make();

    EXPECT_EQ(theme.find("old", 32), path("pixmaps/old.xpm"));
    EXPECT_EQ(theme.find("vector", 32), path("pixmaps/vector.svg"));
    EXPECT_EQ(theme.find("both", 32),
              path("icons/hicolor/16x16/apps/both.png"));
}

TEST_F(IconThemeTest, files_right_in_the_icon_directory)
{
    index("hicolor", HICOLOR);
    touch("icons/local.png");
    touch("icons/themed.png");
    touch("icons/hicolor/16x16/apps/themed.png");
    touch("pixmaps/themed.png");
    const auto theme = make();

    EXPECT_EQ(theme.find("local", 48), path("icons/local.png"));
    // a theme and then a plain file in the icon directory, then pixmaps
    EXPECT_EQ(theme.find("themed", 48),
              path("icons/hicolor/16x16/apps/themed.png"));
}

TEST_F(IconThemeTest, plain_icon_directory_before_pixmaps)
{
    index("hicolor", HICOLOR);
    touch("icons/both.png");
    touch("pixmaps/both.png");
    EXPECT_EQ(make().find("both", 48), path("icons/both.png"));
}

TEST_F(IconThemeTest, absolute_name_and_extension)
{
    index("hicolor", HICOLOR);
    touch("elsewhere/pic.png");
    touch("icons/hicolor/16x16/apps/app.png");
    const auto theme = make();

    EXPECT_EQ(theme.find(path("elsewhere/pic.png"), 16),
              path("elsewhere/pic.png"));
    EXPECT_EQ(theme.find(path("elsewhere/none.png"), 16), "");
    EXPECT_EQ(theme.find("app.png", 16),
              path("icons/hicolor/16x16/apps/app.png"));
    EXPECT_EQ(theme.find("", 16), "");
}

TEST_F(IconThemeTest, missing_theme_and_missing_icon)
{
    touch("pixmaps/only.png");
    const auto theme = make("nothing");

    EXPECT_EQ(theme.find("absent", 16), "");
    EXPECT_EQ(theme.find("only", 16), path("pixmaps/only.png"));
}

TEST_F(IconThemeTest, the_result_is_cached)
{
    index("hicolor", HICOLOR);
    touch("icons/hicolor/16x16/apps/app.png");
    const auto theme = make();

    const auto first = theme.find("app", 16);
    fs::remove(first);
    EXPECT_EQ(theme.find("app", 16), first);
    EXPECT_EQ(theme.find("app", 48), ""); // another size is another question
}

TEST_F(IconThemeTest, theme_in_more_base_directories)
{
    index("hicolor", HICOLOR);
    touch("other/hicolor/16x16/apps/app.png");
    const tiny::IconTheme theme({path("icons"), path("other")}, {});
    EXPECT_EQ(theme.find("app", 16), path("other/hicolor/16x16/apps/app.png"));
}

TEST_F(IconThemeTest, broken_index_is_ignored)
{
    index("hicolor", "garbage\n[Icon Theme]\nDirectories=a,missing\n"
                     "[a]\nSize=abc\nType=Fixed\n");
    touch("icons/hicolor/a/app.png");
    EXPECT_EQ(make().find("app", 16), path("icons/hicolor/a/app.png"));
}
