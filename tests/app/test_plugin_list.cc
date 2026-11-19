#include <gtest/gtest.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "app/plugin_list.h"

namespace fs = std::filesystem;
using tiny::app::plugin_changes;
using tiny::app::plugin_rows;
using tiny::app::remove_plugin_directory;
using tiny::plugin::Origin;
using tiny::plugin::PluginInfo;
using tiny::wm::Config;
using Row = tiny::app::SettingsForm::PluginRow;

namespace {

PluginInfo make_info(const std::string& id, Origin origin)
{
    PluginInfo info = tiny::plugin::parse_plugin_info(
            id, "/nonexistent", origin,
            "[plugin]\nname = " + id +
                    "\nversion = 1.5\nenabled = yes\n"
                    "settings = units;days\n"
                    "[setting.units]\ntype = choice\nchoices = a;b\ndefault = "
                    "a\n"
                    "[setting.days]\ntype = number\nmin = 1\nmax = 7\ndefault "
                    "= 3\n");
    info.problem.clear(); // there is no plugin.lua in the directory
    return info;
}

class TempDir {
  public:
    TempDir()
    {
        static int counter = 0;
        path = fs::temp_directory_path() /
               ("tiny-list-" + std::to_string(getpid()) + "-" +
                std::to_string(counter++));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    fs::path path;
};

} // namespace

TEST(PluginListTest, a_row_tells_what_the_plugin_is_and_what_it_does_now)
{
    PluginInfo system = make_info("sys", Origin::System);
    PluginInfo user = make_info("mine", Origin::User);
    PluginInfo over = make_info("both", Origin::User);
    over.shadows = true;
    over.shadowed_version = "1.0";
    PluginInfo broken = make_info("broken", Origin::System);
    broken.problem = "plugin.lua is missing";
    Config config;
    config.plugins["mine"].enabled = false;
    config.plugins["sys"].values["days"] = "5";
    config.plugins["sys"].values["units"] = "wrong"; // not a choice

    const auto rows = plugin_rows({system, user, over, broken}, config);
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0].id, "sys");
    EXPECT_EQ(rows[0].name, "sys");
    EXPECT_EQ(rows[0].version, "1.5");
    EXPECT_EQ(rows[0].origin, "System");
    EXPECT_FALSE(rows[0].user);
    EXPECT_TRUE(rows[0].enabled); // its own default
    EXPECT_EQ(rows[0].values, (std::vector<std::string>{"a", "5"}));
    EXPECT_EQ(rows[0].specs.size(), 2u);
    EXPECT_EQ(rows[1].origin, "User");
    EXPECT_TRUE(rows[1].user);
    EXPECT_FALSE(rows[1].enabled); // the user switched it off
    EXPECT_EQ(rows[2].origin, "User, replaces 1.0");
    EXPECT_EQ(rows[3].reason, "plugin.lua is missing");
    EXPECT_FALSE(rows[3].enabled);
    over.shadowed_version.clear();
    EXPECT_EQ(plugin_rows({over}, config)[0].origin,
              "User, replaces the system one");
}

TEST(PluginListTest, only_what_changed_is_written)
{
    const auto before = plugin_rows(
            {make_info("a", Origin::System), make_info("b", Origin::System)},
            Config());
    auto after = before;
    EXPECT_TRUE(plugin_changes(before, after).empty());

    after[0].enabled = false;
    after[0].values[0] = "b";
    after[1].values[1] = "7";
    const auto changes = plugin_changes(before, after);
    ASSERT_EQ(changes.size(), 3u);
    EXPECT_EQ(changes[0].section, "plugin.a");
    EXPECT_EQ(changes[0].key, "enabled");
    EXPECT_EQ(changes[0].value, "no");
    EXPECT_EQ(changes[1].key, "units");
    EXPECT_EQ(changes[1].value, "b");
    EXPECT_EQ(changes[2].section, "plugin.b");
    EXPECT_EQ(changes[2].key, "days");
    EXPECT_EQ(changes[2].value, "7");

    after[0].enabled = true;
    EXPECT_EQ(plugin_changes(before, after).size(), 2u); // not "yes" again
}

TEST(PluginListTest, a_plugin_that_is_removed_is_not_written)
{
    const auto before = plugin_rows({make_info("a", Origin::User)}, Config());
    auto after = before;
    after[0].enabled = false;
    after[0].remove = true;
    EXPECT_TRUE(plugin_changes(before, after).empty());
    EXPECT_TRUE(plugin_changes(before, {}).empty()); // lists that differ
}

TEST(PluginListTest, removes_the_directory_of_a_plugin)
{
    const TempDir dir;
    fs::create_directories(dir.path / "gone" / "lib");
    std::ofstream(dir.path / "gone" / "plugin.lua") << "x";
    fs::create_directories(dir.path / "stays");
    std::string error;
    EXPECT_TRUE(remove_plugin_directory(dir.path.string(), "gone", &error))
            << error;
    EXPECT_FALSE(fs::exists(dir.path / "gone"));
    EXPECT_TRUE(fs::exists(dir.path / "stays"));
    EXPECT_TRUE(remove_plugin_directory(dir.path.string(), "gone")); // again
}

TEST(PluginListTest, a_link_is_removed_and_not_what_it_points_to)
{
    const TempDir dir;
    const TempDir other;
    std::ofstream(other.path / "plugin.lua") << "x";
    fs::create_directory_symlink(other.path, dir.path / "linked");
    EXPECT_TRUE(remove_plugin_directory(dir.path.string(), "linked"));
    EXPECT_FALSE(fs::is_symlink(dir.path / "linked"));
    EXPECT_TRUE(fs::exists(other.path / "plugin.lua"));
}

TEST(PluginListTest, nothing_outside_of_the_directory_is_removed)
{
    const TempDir dir;
    const TempDir other;
    std::string error;
    EXPECT_FALSE(remove_plugin_directory(dir.path.string(), "../x", &error));
    EXPECT_NE(error.find("wrong plugin"), std::string::npos);
    EXPECT_FALSE(remove_plugin_directory("", "a", &error));
    EXPECT_FALSE(remove_plugin_directory(dir.path.string(), "", &error));
    EXPECT_FALSE(remove_plugin_directory(dir.path.string(), "A b", &error));
    // a plain file is not a plugin
    std::ofstream(dir.path / "file") << "x";
    EXPECT_FALSE(remove_plugin_directory(dir.path.string(), "file", &error));
    EXPECT_NE(error.find("not a directory"), std::string::npos);
    EXPECT_TRUE(fs::exists(dir.path / "file"));
}

TEST(PluginListTest, a_directory_that_can_not_be_deleted_is_an_error)
{
    if (geteuid() == 0) {
        GTEST_SKIP() << "root can delete everything";
    }
    const TempDir dir;
    fs::create_directories(dir.path / "locked" / "inner");
    std::ofstream(dir.path / "locked" / "inner" / "f") << "x";
    fs::permissions(dir.path / "locked" / "inner", fs::perms::none);
    std::string error;
    EXPECT_FALSE(remove_plugin_directory(dir.path.string(), "locked", &error));
    EXPECT_NE(error.find("can't remove"), std::string::npos);
    fs::permissions(dir.path / "locked" / "inner", fs::perms::owner_all);
}
