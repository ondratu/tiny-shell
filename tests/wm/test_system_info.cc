#include <gtest/gtest.h>

#include "wm/system_info.h"

using tiny::wm::os_release_name;
using tiny::wm::os_release_value;
using tiny::wm::system_info;

TEST(OsReleaseTest, a_value_in_double_quotes)
{
    EXPECT_EQ(
            os_release_value("NAME=\"Debian GNU/Linux\"\nID=debian\n", "NAME"),
            "Debian GNU/Linux");
}

TEST(OsReleaseTest, a_value_without_quotes_or_in_single_quotes)
{
    EXPECT_EQ(os_release_value("ID=debian\n", "ID"), "debian");
    EXPECT_EQ(os_release_value("ID='alpine'\n", "ID"), "alpine");
}

TEST(OsReleaseTest, a_backslash_protects_the_next_character)
{
    EXPECT_EQ(os_release_value("NAME=\"A \\\"B\\\" C\"\n", "NAME"),
              "A \"B\" C");
}

TEST(OsReleaseTest, a_key_is_matched_whole)
{
    EXPECT_EQ(os_release_value("PRETTY_NAME=x\nNAME=y\n", "NAME"), "y");
    EXPECT_EQ(os_release_value("NAMES=x\n", "NAME"), "");
    EXPECT_EQ(os_release_value("", "NAME"), "");
}

TEST(OsReleaseTest, the_last_line_of_a_key_wins_and_blanks_are_ignored)
{
    EXPECT_EQ(os_release_value("  A=1\nA=2  \n", "A"), "2");
}

TEST(OsReleaseTest, the_pretty_name_is_the_name_of_the_system)
{
    EXPECT_EQ(os_release_name("NAME=\"X\"\nPRETTY_NAME=\"X Linux 1\"\n"),
              "X Linux 1");
}

TEST(OsReleaseTest, without_a_pretty_name_the_name_and_the_version_are_used)
{
    EXPECT_EQ(os_release_name("NAME=Alpine\nVERSION=3.20\n"), "Alpine 3.20");
    EXPECT_EQ(os_release_name("NAME=Arch\n"), "Arch");
    EXPECT_EQ(os_release_name("ID=x\n"), "");
}

TEST(SystemInfoTest, the_system_this_runs_on_is_known)
{
    const tiny::wm::SystemInfo info = system_info();

    EXPECT_FALSE(info.name.empty());
    EXPECT_FALSE(info.kernel.empty());
    EXPECT_FALSE(info.machine.empty());
}
