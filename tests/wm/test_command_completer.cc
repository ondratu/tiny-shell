#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include <sys/stat.h>
#include <unistd.h>

#include "wm/command_completer.h"

using tiny::wm::CommandCompleter;

namespace fs = std::filesystem;

namespace {

CommandCompleter with(std::vector<std::string> names)
{
    CommandCompleter completer;
    completer.set_commands(std::move(names));
    return completer;
}

} // namespace

TEST(CommandCompleterTest, a_single_program_is_completed_with_a_blank)
{
    auto completer = with({"firefox", "xterm", "gimp"});

    EXPECT_EQ(completer.next("fir"), "firefox ");
}

TEST(CommandCompleterTest, the_first_tab_adds_what_the_programs_share)
{
    auto completer = with({"gcc", "gcc-12", "gcc-ar", "gdb"});

    EXPECT_EQ(completer.next("gc"), "gcc"); // gdb is not a match
    auto other = with({"gnome-shell", "gnome-terminal", "gnuplot"});
    EXPECT_EQ(other.next("g"), "gn");
    EXPECT_EQ(other.next("gn"), "gnome-shell"); // nothing to add: the first
}

TEST(CommandCompleterTest, the_next_tabs_walk_through_the_programs)
{
    auto completer = with({"gcc", "gcc-12", "gcc-ar"});

    EXPECT_EQ(completer.next("g"), "gcc");
    EXPECT_EQ(completer.next("gcc"), "gcc");
    EXPECT_EQ(completer.next("gcc"), "gcc-12");
    EXPECT_EQ(completer.next("gcc-12"), "gcc-ar");
    EXPECT_EQ(completer.next("gcc-ar"), "gcc"); // and round
}

TEST(CommandCompleterTest, without_a_common_part_the_first_tab_takes_a_program)
{
    auto completer = with({"gdb", "gimp", "git"});

    EXPECT_EQ(completer.next("g"), "gdb");
    EXPECT_EQ(completer.next("gdb"), "gimp");
}

TEST(CommandCompleterTest, a_change_of_the_text_starts_again)
{
    auto completer = with({"gdb", "gimp", "git", "xterm"});

    EXPECT_EQ(completer.next("g"), "gdb");
    EXPECT_EQ(completer.next("gi"), "gimp"); // gimp and git share only "gi"
    EXPECT_EQ(completer.next("gimp"), "git");
    EXPECT_EQ(completer.next("x"), "xterm ");
}

TEST(CommandCompleterTest, a_completed_program_stays_as_it_is)
{
    auto completer = with({"firefox", "fish"});

    EXPECT_EQ(completer.next("fire"), "firefox ");
    EXPECT_EQ(completer.next("firefox "), "firefox ");
}

TEST(CommandCompleterTest, an_unknown_word_empty_text_and_arguments_are_left)
{
    auto completer = with({"firefox"});

    EXPECT_EQ(completer.next("zzz"), "zzz");
    EXPECT_EQ(completer.next(""), "");
    EXPECT_EQ(completer.next("firefox --pri"), "firefox --pri");
    EXPECT_EQ(completer.next("ls fi"), "ls fi");
}

TEST(CommandCompleterTest, the_case_of_the_letters_counts)
{
    auto completer = with({"Xorg", "xterm"});

    EXPECT_EQ(completer.next("x"), "xterm ");
    EXPECT_EQ(completer.next("X"), "Xorg ");
}

TEST(CommandCompleterTest, the_programs_can_be_set_again)
{
    CommandCompleter completer;
    EXPECT_EQ(completer.next("fi"), "fi");

    completer.set_commands({"fish", "fish", "find"});

    EXPECT_EQ(completer.next("fis"), "fish ");
    EXPECT_EQ(completer.next("fi"), "find"); // find, fish: the same start
}

class ListCommandsTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-test-path-" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "one");
        fs::create_directories(root / "two");
        fs::create_directories(root / "one" / "subdir");
    }

    void TearDown() override { fs::remove_all(root); }

    void file(const fs::path& path, mode_t mode)
    {
        std::ofstream(path) << "#!/bin/sh\n";
        chmod(path.c_str(), mode);
    }

    fs::path root;
};

TEST_F(ListCommandsTest, lists_the_programs_of_all_the_directories_sorted)
{
    file(root / "one" / "beta", 0755);
    file(root / "one" / "alpha", 0755);
    file(root / "two" / "gamma", 0755);

    const auto names = tiny::wm::list_commands(
            root.string() + "/one:" + root.string() + "/two");

    EXPECT_EQ(names, (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

TEST_F(ListCommandsTest, a_name_in_two_directories_is_listed_once)
{
    file(root / "one" / "tool", 0755);
    file(root / "two" / "tool", 0755);

    const auto names = tiny::wm::list_commands(
            root.string() + "/one:" + root.string() + "/two");

    EXPECT_EQ(names, std::vector<std::string>{"tool"});
}

TEST_F(ListCommandsTest, files_that_can_not_run_and_directories_are_left_out)
{
    file(root / "one" / "script", 0755);
    file(root / "one" / "readme", 0644);

    const auto names = tiny::wm::list_commands(root.string() + "/one");

    EXPECT_EQ(names, std::vector<std::string>{"script"});
}

TEST_F(ListCommandsTest, a_missing_directory_and_empty_items_are_skipped)
{
    file(root / "one" / "tool", 0755);

    const auto names = tiny::wm::list_commands(
            root.string() + "/nothing::" + root.string() + "/one:");

    EXPECT_EQ(names, std::vector<std::string>{"tool"});
    EXPECT_TRUE(tiny::wm::list_commands("").empty());
}
