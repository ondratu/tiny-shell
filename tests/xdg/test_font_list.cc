#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "xdg/font_list.h"

using tiny::xdg::font_families;
using tiny::xdg::font_sample;
using tiny::xdg::sort_families;
using Names = std::vector<std::string>;

TEST(FontListTest, the_families_are_sorted_without_regard_to_case)
{
    EXPECT_EQ(
            sort_families(
                    {"Noto Serif", "dejavu sans", "Cantarell", "DejaVu Serif"}),
            (Names{"Cantarell", "dejavu sans", "DejaVu Serif", "Noto Serif"}));
}

TEST(FontListTest, a_family_is_listed_once)
{
    EXPECT_EQ(sort_families({"B", "A", "B", "A", "B"}), (Names{"A", "B"}));
}

TEST(FontListTest, a_spelling_that_differs_in_case_is_a_name_of_its_own)
{
    EXPECT_EQ(sort_families({"mono", "Mono", "mono"}), (Names{"Mono", "mono"}));
}

TEST(FontListTest, a_shorter_name_comes_before_the_longer_one)
{
    EXPECT_EQ(sort_families({"Noto Sans Mono", "Noto Sans", "Noto"}),
              (Names{"Noto", "Noto Sans", "Noto Sans Mono"}));
}

TEST(FontListTest, empty_and_hidden_names_are_dropped)
{
    EXPECT_EQ(sort_families({"", ".Hidden", "Shown", "."}), (Names{"Shown"}));
    EXPECT_TRUE(sort_families({}).empty());
}

TEST(FontListTest, the_installed_families_are_ready_to_show)
{
    // a system may have no fonts at all; what is there is sorted and clean
    const Names families = font_families();

    EXPECT_EQ(sort_families(families), families);
}

namespace {

//! A set of the characters, closed when the test ends
struct Chars {
    FcCharSet* set = FcCharSetCreate();
    Chars(std::initializer_list<FcChar32> list)
    {
        for (const FcChar32 c : list) {
            FcCharSetAddChar(set, c);
        }
    }
    Chars(const Chars&) = delete;
    Chars& operator=(const Chars&) = delete;
    ~Chars() { FcCharSetDestroy(set); }
};

} // namespace

TEST(FontSampleTest, the_first_characters_in_the_order_of_their_codes)
{
    const Chars chars{0x3042, 'b', 'a', 0xE000};

    EXPECT_EQ(font_sample(chars.set, 0, 3), "ab\xE3\x81\x82");
    EXPECT_EQ(font_sample(chars.set, 0, 10), "ab\xE3\x81\x82\xEE\x80\x80");
}

TEST(FontSampleTest, skipped_characters_are_not_counted)
{
    const Chars chars{'a', 'b', 'c', 'd'};

    EXPECT_EQ(font_sample(chars.set, 1, 2), "bc");
    EXPECT_EQ(font_sample(chars.set, 3, 5), "d");
    EXPECT_EQ(font_sample(chars.set, 4, 5), "");
    EXPECT_EQ(font_sample(chars.set, 0, 0), "");
}

TEST(FontSampleTest, spaces_and_controls_are_not_a_sample)
{
    const Chars chars{0x01, ' ', 0x7f, 0x85, 0xA0, 0xAD, 0xD7FF, 0xFFFE, 'x'};

    // only x and U+D7FF are left
    EXPECT_EQ(font_sample(chars.set, 0, 5), "x\xED\x9F\xBF");
}

TEST(FontSampleTest, characters_of_every_length_are_encoded)
{
    const Chars chars{0xE9, 0x20AC, 0x1F600};

    EXPECT_EQ(font_sample(chars.set, 0, 3),
              "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
}

TEST(FontSampleTest, nothing_without_a_set)
{
    EXPECT_EQ(font_sample(nullptr, 0, 3), "");
    const Chars none{};
    EXPECT_EQ(font_sample(none.set, 0, 3), "");
}
