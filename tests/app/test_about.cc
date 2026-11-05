#include <gtest/gtest.h>

#include "app/about.h"

using tiny::app::about_lines;

TEST(AboutTest, tells_the_version_the_system_the_license_and_the_address)
{
    const auto lines =
            about_lines("1.2.3", {"Test Linux 9", "Linux 6.1", "x86_64"});

    ASSERT_EQ(lines.size(), 6u);
    EXPECT_EQ(lines[0], "A small window manager and shell for X11");
    EXPECT_EQ(lines[1], "Version 1.2.3");
    EXPECT_EQ(lines[2], "Test Linux 9");
    EXPECT_EQ(lines[3], "Linux 6.1 (x86_64)");
    EXPECT_EQ(lines[4], tiny::app::about_license);
    EXPECT_EQ(lines[5], tiny::app::about_project);
}

TEST(AboutTest, an_unknown_system_is_told_and_the_kernel_is_left_out)
{
    const auto lines = about_lines("1", {});

    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[2], "System unknown");
    EXPECT_EQ(lines[3], tiny::app::about_license);
}

TEST(AboutTest, the_kernel_without_the_machine_has_no_brackets)
{
    const auto lines = about_lines("1", {"X", "Linux 6.1", ""});

    EXPECT_EQ(lines[3], "Linux 6.1");
}
