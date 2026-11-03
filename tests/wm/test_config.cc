#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "wm/config.h"

using tiny::wm::Config;
using tiny::wm::parse_config;
using Strings = std::vector<std::string>;

TEST(ConfigTest, the_defaults_work_without_any_file)
{
    const Config c = parse_config({});

    EXPECT_EQ(c.scale, 1.0);
    EXPECT_EQ(c.reboot_command, "systemctl reboot");
    EXPECT_EQ(c.poweroff_command, "systemctl poweroff");
    EXPECT_EQ(c.terminal, (Strings{"xterm", "-e"}));
}

TEST(ConfigTest, reads_the_settings)
{
    const Config c = parse_config(
            {{"a",
              "[theme]\nscale = 1.5\n"
              "[commands]\nreboot = sudo reboot\npoweroff = doas poweroff\n"
              "terminal = foot -e\n"}});

    EXPECT_EQ(c.scale, 1.5);
    EXPECT_EQ(c.reboot_command, "sudo reboot");
    EXPECT_EQ(c.poweroff_command, "doas poweroff");
    EXPECT_EQ(c.terminal, (Strings{"foot", "-e"}));
}

TEST(ConfigTest, a_terminal_with_quotes_is_split_like_an_exec)
{
    const Config c = parse_config(
            {{"a", "[commands]\nterminal = \"my term\" --x -e\n"}});

    EXPECT_EQ(c.terminal, (Strings{"my term", "--x", "-e"}));
}

TEST(ConfigTest, a_later_source_wins_key_by_key)
{
    const Config c = parse_config(
            {{"global", "[theme]\nscale = 2\n[commands]\nreboot = a\n"},
             {"user", "[commands]\nreboot = b\npoweroff = c\n"}});

    EXPECT_EQ(c.scale, 2.0);
    EXPECT_EQ(c.reboot_command, "b");
    EXPECT_EQ(c.poweroff_command, "c");
}

TEST(ConfigTest, a_wrong_value_keeps_the_value_of_the_lower_source)
{
    for (const char* bad : {"", "abc", "0", "-2", "2x", "1000", "nan"}) {
        Strings problems;
        const Config c = parse_config(
                {{"global", "[theme]\nscale = 2\n"},
                 {"user", std::string("[theme]\nscale = ") + bad + "\n"}},
                &problems);

        EXPECT_EQ(c.scale, 2.0) << bad;
        ASSERT_EQ(problems.size(), 1u) << bad;
        EXPECT_NE(problems[0].find("scale"), std::string::npos);
    }
}

TEST(ConfigTest, an_empty_command_is_a_problem_and_keeps_the_default)
{
    Strings problems;
    const Config c = parse_config(
            {{"a", "[commands]\nreboot =\npoweroff =\nterminal =\n"}},
            &problems);

    EXPECT_EQ(c.reboot_command, "systemctl reboot");
    EXPECT_EQ(c.poweroff_command, "systemctl poweroff");
    EXPECT_EQ(c.terminal, (Strings{"xterm", "-e"}));
    EXPECT_EQ(problems.size(), 3u);
}

TEST(ConfigTest, unknown_keys_and_sections_are_ignored)
{
    Strings problems;
    const Config c = parse_config(
            {{"a", "[plugins]\nclock = on\n[theme]\nfont = Sans\nscale = 3\n"}},
            &problems);

    EXPECT_EQ(c.scale, 3.0);
    EXPECT_TRUE(problems.empty());
}

TEST(ConfigTest, a_wrong_line_is_told_with_the_name_of_the_file)
{
    Strings problems;
    parse_config({{"/x/config", "[theme]\nnonsense\n"}}, &problems);

    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems[0].rfind("/x/config:2: ", 0), 0u);
}

TEST(ConfigTest, problems_may_be_ignored)
{
    EXPECT_EQ(parse_config({{"a", "nonsense\n[theme]\nscale = x\n"}}).scale,
              1.0);
}

TEST(ConfigTest, the_files_go_from_the_lowest_priority_to_the_user)
{
    EXPECT_EQ(tiny::wm::config_files("/h/.config", "/etc/xdg:/usr/etc", "/h"),
              (Strings{"/usr/etc/tiny-shell.ini", "/etc/xdg/tiny-shell.ini",
                       "/h/.config/tiny-shell.ini"}));
    EXPECT_EQ(
            tiny::wm::config_files(nullptr, nullptr, "/h"),
            (Strings{"/etc/xdg/tiny-shell.ini", "/h/.config/tiny-shell.ini"}));
}

class ConfigFilesTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        char path[] = "/tmp/tiny-config-XXXXXX";
        ASSERT_NE(mkdtemp(path), nullptr);
        dir = path;
    }

    void TearDown() override
    {
        for (const std::string& file : written) {
            unlink(file.c_str());
        }
        rmdir(dir.c_str());
    }

    std::string write(const std::string& name, const std::string& text)
    {
        const std::string file = dir + "/" + name;
        std::ofstream(file) << text;
        written.push_back(file);
        return file;
    }

    std::string dir;
    Strings written;
};

TEST_F(ConfigFilesTest, load_layers_the_files_and_skips_the_missing_ones)
{
    const std::string first = write("global", "[theme]\nscale = 2\n");
    const std::string second = write("user", "[commands]\nreboot = r\n");

    Strings problems;
    const Config c =
            tiny::wm::load_config({first, dir + "/missing", second}, &problems);

    EXPECT_EQ(c.scale, 2.0);
    EXPECT_EQ(c.reboot_command, "r");
    EXPECT_TRUE(problems.empty());
}

TEST_F(ConfigFilesTest, load_names_the_file_of_a_problem)
{
    const std::string file = write("config", "oops\n");

    Strings problems;
    tiny::wm::load_config({file}, &problems);

    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems[0].rfind(file + ":1: ", 0), 0u);
}

TEST_F(ConfigFilesTest, no_file_means_the_defaults)
{
    EXPECT_EQ(tiny::wm::load_config({dir + "/missing"}).scale, 1.0);
}
