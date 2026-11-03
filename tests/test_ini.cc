#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ini.h"

using tiny::Ini;

TEST(IniTest, reads_keys_of_sections)
{
    Ini ini;
    ini.parse("[theme]\nscale = 1.5\n[commands]\nreboot=sudo reboot\n");

    EXPECT_EQ(ini.get("theme", "scale"), "1.5");
    EXPECT_EQ(ini.get("commands", "reboot"), "sudo reboot");
    EXPECT_FALSE(ini.get("theme", "reboot"));
    EXPECT_FALSE(ini.get("other", "scale"));
}

TEST(IniTest, an_empty_text_has_no_values)
{
    Ini ini;
    EXPECT_TRUE(ini.empty());
    ini.parse("\n  \n# only a comment\n");
    EXPECT_TRUE(ini.empty());
    ini.parse("a = b\n");
    EXPECT_FALSE(ini.empty());
}

TEST(IniTest, blanks_around_names_and_values_are_dropped)
{
    Ini ini;
    ini.parse("  [ theme ]  \r\n\t scale \t=\t 2 \r\n");

    EXPECT_EQ(ini.get("theme", "scale"), "2");
}

TEST(IniTest, comments_are_whole_lines)
{
    Ini ini;
    ini.parse("# a\n; b\n[s]\n  # c\nkey = value # not a comment\n");

    EXPECT_EQ(ini.get("s", "key"), "value # not a comment");
}

TEST(IniTest, the_value_is_everything_after_the_first_equals)
{
    Ini ini;
    ini.parse("[s]\ncmd = env A=b run\nempty =\n");

    EXPECT_EQ(ini.get("s", "cmd"), "env A=b run");
    EXPECT_EQ(ini.get("s", "empty"), "");
}

TEST(IniTest, a_key_before_a_section_is_in_the_unnamed_one)
{
    Ini ini;
    ini.parse("a = 1\n[s]\na = 2\n");

    EXPECT_EQ(ini.get("", "a"), "1");
    EXPECT_EQ(ini.get("s", "a"), "2");
}

TEST(IniTest, names_are_case_sensitive)
{
    Ini ini;
    ini.parse("[S]\nKey = 1\n");

    EXPECT_FALSE(ini.get("s", "Key"));
    EXPECT_FALSE(ini.get("S", "key"));
    EXPECT_EQ(ini.get("S", "Key"), "1");
}

TEST(IniTest, a_later_value_replaces_the_earlier_one_even_in_another_text)
{
    Ini ini;
    ini.parse("[s]\na = 1\nb = 1\na = 2\n");
    ini.parse("[s]\nb = 3\n");

    EXPECT_EQ(ini.get("s", "a"), "2");
    EXPECT_EQ(ini.get("s", "b"), "3");
}

TEST(IniTest, the_last_line_needs_no_newline)
{
    Ini ini;
    ini.parse("[s]\na = 1");

    EXPECT_EQ(ini.get("s", "a"), "1");
}

TEST(IniTest, a_wrong_line_is_skipped_and_described)
{
    Ini ini;
    std::vector<std::string> errors;
    ini.parse("[s\nno equals\n= value\n[ok]\na = 1\n", &errors);

    ASSERT_EQ(errors.size(), 3u);
    EXPECT_EQ(errors[0].rfind("1: ", 0), 0u);
    EXPECT_EQ(errors[1].rfind("2: ", 0), 0u);
    EXPECT_EQ(errors[2].rfind("3: ", 0), 0u);
    EXPECT_EQ(ini.get("ok", "a"), "1");
}

TEST(IniTest, a_comment_may_follow_a_section)
{
    Ini ini;
    std::vector<std::string> errors;
    ini.parse("[a] # x\nk = 1\n[b];x\nk = 2\n[c] text\nk = 3\n", &errors);

    EXPECT_EQ(ini.get("a", "k"), "1");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].rfind("5: ", 0), 0u);
    // the keys of a broken section stay in the section before it
    EXPECT_EQ(ini.get("b", "k"), "3");
}

TEST(IniTest, a_byte_order_mark_is_skipped)
{
    Ini ini;
    ini.parse("\xEF\xBB\xBF[s]\na = 1\n");

    EXPECT_EQ(ini.get("s", "a"), "1");
}

TEST(IniTest, errors_may_be_ignored)
{
    Ini ini;
    ini.parse("garbage\n");
    EXPECT_TRUE(ini.empty());
}

TEST(IniTest, trim_drops_the_blanks)
{
    EXPECT_EQ(tiny::trim(" \t x y \r\n"), "x y");
    EXPECT_EQ(tiny::trim(" \t"), "");
    EXPECT_EQ(tiny::trim(""), "");
}

TEST(IniTest, set_adds_and_replaces_a_value)
{
    Ini ini;
    ini.set("s", "a", "1");
    ini.set("s", "a", "2");
    ini.set("t", "b", "3");

    EXPECT_EQ(ini.get("s", "a"), "2");
    EXPECT_EQ(ini.get("t", "b"), "3");
}

TEST(IniTest, dump_writes_the_sections_in_order_after_the_keys_without_one)
{
    Ini ini;
    ini.parse("[b]\nx = 1\n[a]\ny = 2 # not a comment\n");
    ini.set("", "top", "0");

    EXPECT_EQ(ini.dump(),
              "top = 0\n\n[a]\ny = 2 # not a comment\n\n[b]\nx = 1\n");
}

TEST(IniTest, dump_of_nothing_is_empty)
{
    EXPECT_EQ(Ini().dump(), "");
}

TEST(IniTest, the_dump_is_read_back_the_same)
{
    Ini ini;
    ini.parse("a = 1\n[s]\nk = v;w\nempty =\n[t]\nk = x = y\n");

    Ini again;
    again.parse(ini.dump());

    EXPECT_EQ(again.dump(), ini.dump());
    EXPECT_EQ(again.get("s", "empty"), "");
    EXPECT_EQ(again.get("t", "k"), "x = y");
    EXPECT_EQ(again.get("", "a"), "1");
}
