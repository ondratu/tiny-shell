#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "xdg/dirs.h"

using tiny::xdg::config_dir_list;
using Dirs = std::vector<std::string>;

TEST(ConfigDirsTest, defaults_are_the_home_and_etc_xdg)
{
    EXPECT_EQ(config_dir_list(nullptr, nullptr, "/home/u"),
              (Dirs{"/home/u/.config", "/etc/xdg"}));
    EXPECT_EQ(config_dir_list("", "", "/home/u"),
              (Dirs{"/home/u/.config", "/etc/xdg"}));
}

TEST(ConfigDirsTest, the_variables_win_over_the_defaults)
{
    EXPECT_EQ(config_dir_list("/c", "/a:/b/", "/home/u"),
              (Dirs{"/c", "/a", "/b"}));
}

TEST(ConfigDirsTest, relative_and_repeated_directories_are_dropped)
{
    EXPECT_EQ(config_dir_list("rel", "/a:rel:/a", "/home/u"),
              (Dirs{"/home/u/.config", "/a"}));
}

TEST(ConfigDirsTest, without_a_home_only_the_system_directories_are_left)
{
    EXPECT_EQ(config_dir_list(nullptr, nullptr, nullptr), (Dirs{"/etc/xdg"}));
}
