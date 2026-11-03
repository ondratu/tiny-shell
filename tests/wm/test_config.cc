#include <gtest/gtest.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iterator>
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
    EXPECT_TRUE(c.favorites.empty());
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

TEST(ConfigTest,
     the_favorites_are_a_list_without_blanks_empty_items_and_repeats)
{
    const Config c = parse_config(
            {{"a", "[dock]\nfavorites = a.desktop; b.desktop;;a.desktop ;\n"}});

    EXPECT_EQ(c.favorites, (Strings{"a.desktop", "b.desktop"}));
}

TEST(ConfigTest, the_favorites_of_the_user_replace_the_global_ones)
{
    const Config c = parse_config({{"global", "[dock]\nfavorites = a;b\n"},
                                   {"user", "[dock]\nfavorites =\n"}});

    EXPECT_TRUE(c.favorites.empty());
    EXPECT_EQ(parse_config({{"global", "[dock]\nfavorites = a;b\n"}}).favorites,
              (Strings{"a", "b"}));
}

TEST(ConfigTest, the_file_of_the_user_is_in_the_configuration_home)
{
    EXPECT_EQ(tiny::wm::user_config_file("/c", "/h"), "/c/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file(nullptr, "/h"),
              "/h/.config/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file("relative", "/h"),
              "/h/.config/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file("/", nullptr), "/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file(nullptr, nullptr), "");
    EXPECT_EQ(tiny::wm::user_config_file("", ""), "");
}

TEST_F(ConfigFilesTest, save_favorites_makes_the_file)
{
    const std::string file = dir + "/tiny-shell.ini";
    written.push_back(file);

    std::string error;
    ASSERT_TRUE(
            tiny::wm::save_favorites(file, {"a.desktop", "b.desktop"}, &error))
            << error;

    EXPECT_EQ(tiny::wm::load_config({file}).favorites,
              (Strings{"a.desktop", "b.desktop"}));
    EXPECT_NE(access((file + ".tmp").c_str(), F_OK), 0);
}

TEST_F(ConfigFilesTest, save_favorites_keeps_the_values_but_not_the_comments)
{
    const std::string file =
            write("tiny-shell.ini", "# my notes\n[theme]\nscale = 2\n"
                                    "[dock]\nfavorites = old.desktop\n"
                                    "[future]\nkey = value\n");

    ASSERT_TRUE(tiny::wm::save_favorites(file, {"new.desktop"}));

    const Config c = tiny::wm::load_config({file});
    EXPECT_EQ(c.scale, 2.0);
    EXPECT_EQ(c.favorites, (Strings{"new.desktop"}));
    std::ifstream in(file);
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    EXPECT_EQ(text.find("my notes"), std::string::npos);
    EXPECT_NE(text.find("[future]\nkey = value\n"), std::string::npos);
}

TEST_F(ConfigFilesTest, save_favorites_writes_an_empty_list)
{
    const std::string file = write("tiny-shell.ini", "[dock]\nfavorites = a\n");

    ASSERT_TRUE(tiny::wm::save_favorites(file, {}));

    EXPECT_TRUE(tiny::wm::load_config({file}).favorites.empty());
}

TEST_F(ConfigFilesTest, save_favorites_makes_the_directory_of_the_file)
{
    const std::string file = dir + "/config/tiny-shell.ini";

    ASSERT_TRUE(tiny::wm::save_favorites(file, {"a"}));

    EXPECT_EQ(tiny::wm::load_config({file}).favorites, (Strings{"a"}));
    unlink(file.c_str());
    rmdir((dir + "/config").c_str());
}

TEST_F(ConfigFilesTest, save_favorites_refuses_an_id_that_breaks_the_file)
{
    const std::string file = write("tiny-shell.ini", "[dock]\nfavorites = a\n");

    for (const char* id : {"", "a;b", "a\nb", "a\rb"}) {
        std::string error;
        EXPECT_FALSE(tiny::wm::save_favorites(file, {id}, &error)) << id;
        EXPECT_FALSE(error.empty());
    }
    EXPECT_EQ(tiny::wm::load_config({file}).favorites, (Strings{"a"}));
}

TEST_F(ConfigFilesTest, save_favorites_tells_when_the_file_cannot_be_written)
{
    std::string error;

    EXPECT_FALSE(tiny::wm::save_favorites(dir + "/missing/x/tiny-shell.ini",
                                          {"a"}, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(
            tiny::wm::save_favorites(dir + "/missing/x/tiny-shell.ini", {"a"}));
}

TEST_F(ConfigFilesTest,
       save_favorites_keeps_the_old_file_when_it_cannot_replace)
{
    // a directory cannot be replaced by a file
    const std::string file = dir + "/tiny-shell.ini";
    ASSERT_EQ(mkdir(file.c_str(), 0777), 0);

    std::string error;
    EXPECT_FALSE(tiny::wm::save_favorites(file, {"a"}, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(access((file + ".tmp").c_str(), F_OK), 0);
    rmdir(file.c_str());
}

TEST_F(ConfigFilesTest, save_favorites_tells_when_the_directory_is_read_only)
{
    if (geteuid() == 0) {
        GTEST_SKIP() << "root can write anywhere";
    }
    const std::string readonly = dir + "/readonly";
    ASSERT_EQ(mkdir(readonly.c_str(), 0500), 0);

    std::string error;
    EXPECT_FALSE(tiny::wm::save_favorites(readonly + "/tiny-shell.ini", {"a"},
                                          &error));
    EXPECT_FALSE(error.empty());
    rmdir(readonly.c_str());
}

TEST_F(ConfigFilesTest, save_favorites_tells_when_the_disk_is_full)
{
    const std::string file = dir + "/tiny-shell.ini";
    // a write over the limit of the size of a file fails (EFBIG)
    struct rlimit old_limit;
    ASSERT_EQ(getrlimit(RLIMIT_FSIZE, &old_limit), 0);
    struct rlimit tiny_limit = old_limit;
    tiny_limit.rlim_cur = 1;
    void (*old_handler)(int) = signal(SIGXFSZ, SIG_IGN);
    ASSERT_EQ(setrlimit(RLIMIT_FSIZE, &tiny_limit), 0);

    std::string error;
    const bool saved = tiny::wm::save_favorites(file, {"a.desktop"}, &error);

    setrlimit(RLIMIT_FSIZE, &old_limit);
    signal(SIGXFSZ, old_handler);
    EXPECT_FALSE(saved);
    EXPECT_FALSE(error.empty());
    EXPECT_NE(access(file.c_str(), F_OK), 0);
    EXPECT_NE(access((file + ".tmp").c_str(), F_OK), 0);
}

TEST_F(ConfigFilesTest, save_favorites_writes_through_a_symbolic_link)
{
    const std::string real = write("real.ini", "[theme]\nscale = 2\n");
    const std::string link = dir + "/link.ini";
    ASSERT_EQ(symlink(real.c_str(), link.c_str()), 0);
    written.push_back(link);

    ASSERT_TRUE(tiny::wm::save_favorites(link, {"a.desktop"}));

    struct stat info;
    ASSERT_EQ(lstat(link.c_str(), &info), 0);
    EXPECT_TRUE(S_ISLNK(info.st_mode));
    const Config c = tiny::wm::load_config({real});
    EXPECT_EQ(c.favorites, (Strings{"a.desktop"}));
    EXPECT_EQ(c.scale, 2.0);
}

TEST_F(ConfigFilesTest, save_favorites_keeps_the_permissions_of_the_file)
{
    const std::string file = write("tiny-shell.ini", "[dock]\nfavorites = a\n");
    ASSERT_EQ(chmod(file.c_str(), 0600), 0);

    ASSERT_TRUE(tiny::wm::save_favorites(file, {"b"}));

    struct stat info;
    ASSERT_EQ(stat(file.c_str(), &info), 0);
    EXPECT_EQ(info.st_mode & 07777, 0600u);
}
