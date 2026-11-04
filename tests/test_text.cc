#include <gtest/gtest.h>

#include "text.h"

using Strings = std::vector<std::string>;

TEST(TextTest, trim_drops_the_blanks_at_both_ends)
{
    EXPECT_EQ(tiny::trim(" \t x y \r\n"), "x y");
    EXPECT_EQ(tiny::trim(" \t"), "");
    EXPECT_EQ(tiny::trim(""), "");
}

TEST(TextTest, split_gives_the_items_and_drops_the_empty_ones)
{
    EXPECT_EQ(tiny::split("a:b::c:", ':'), (Strings{"a", "b", "c"}));
    EXPECT_EQ(tiny::split("", ':'), Strings{});
    EXPECT_EQ(tiny::split(":::", ':'), Strings{});
    EXPECT_EQ(tiny::split("one", ','), (Strings{"one"}));
}

TEST(TextTest, split_keeps_the_blanks_unless_it_is_asked_to_trim)
{
    EXPECT_EQ(tiny::split(" a , b ,  ", ','), (Strings{" a ", " b ", "  "}));
    EXPECT_EQ(tiny::split(" a , b ,  ", ',', true), (Strings{"a", "b"}));
}

TEST(TextTest, lower_changes_the_letters_of_ascii_only)
{
    EXPECT_EQ(tiny::lower("Fire FOX 9"), "fire fox 9");
    EXPECT_EQ(tiny::lower("\xC3\x89"), "\xC3\x89");
    EXPECT_EQ(tiny::lower(""), "");
}
