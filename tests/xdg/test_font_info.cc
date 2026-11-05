#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "xdg/font_info.h"
#include "xdg/font_list.h"

using tiny::xdg::font_file_info;
using tiny::xdg::font_info;
using tiny::xdg::font_name_text;
using tiny::xdg::InfoRow;

namespace {

const InfoRow* find(const std::vector<InfoRow>& rows, const std::string& label)
{
    for (const InfoRow& row : rows) {
        if (row.label == label) {
            return &row;
        }
    }
    return nullptr;
}

} // namespace

TEST(FontNameTextTest, windows_names_are_utf16_big_endian)
{
    EXPECT_EQ(font_name_text(3, std::string("\0A\x01\x1B", 4)), "A\xC4\x9B");
}

TEST(FontNameTextTest, a_pair_of_surrogates_is_one_character)
{
    // U+1F600
    EXPECT_EQ(font_name_text(0, std::string("\xD8\x3D\xDE\x00", 4)),
              "\xF0\x9F\x98\x80");
}

TEST(FontNameTextTest, a_lone_surrogate_is_the_replacement_character)
{
    EXPECT_EQ(font_name_text(3, std::string("\xD8\x3D\x00\x41", 4)),
              "\xEF\xBF\xBD"
              "A");
}

TEST(FontNameTextTest, macintosh_names_are_ascii)
{
    EXPECT_EQ(font_name_text(1, "Dej\x8Fvu"), "Dej?vu");
}

TEST(FontNameTextTest, another_platform_has_no_text)
{
    EXPECT_TRUE(font_name_text(2, "abc").empty());
}

TEST(FontInfoTest, a_family_that_is_not_installed_has_no_rows)
{
    EXPECT_TRUE(font_info("No Such Family 12345").empty());
}

TEST(FontInfoTest, a_file_that_is_not_a_font_has_no_rows)
{
    EXPECT_TRUE(font_file_info("/nonexistent/file.ttf", 0).empty());
    EXPECT_TRUE(font_file_info("/etc/passwd", 0).empty());
}

TEST(FontInfoTest, an_installed_family_is_described)
{
    const std::vector<std::string> families = tiny::xdg::font_families();
    if (families.empty()) {
        GTEST_SKIP() << "no fonts on this system";
    }

    const std::vector<InfoRow> rows = font_info(families.front());

    ASSERT_FALSE(rows.empty());
    ASSERT_NE(find(rows, "Family"), nullptr);
    EXPECT_FALSE(find(rows, "Family")->value.empty());
    EXPECT_NE(find(rows, "Styles"), nullptr);
    EXPECT_NE(find(rows, "Spacing"), nullptr);
    EXPECT_NE(find(rows, "Files"), nullptr);
    EXPECT_TRUE(find(rows, "Files")->heading);
}

TEST(FontInfoTest, the_files_are_listed_after_their_title)
{
    const std::vector<std::string> families = tiny::xdg::font_families();
    if (families.empty()) {
        GTEST_SKIP() << "no fonts on this system";
    }

    const std::vector<InfoRow> rows = font_info(families.front());

    size_t title = rows.size();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].label == "Files") {
            title = i;
        }
    }
    ASSERT_LT(title + 1, rows.size());
    for (size_t i = title + 1; i < rows.size(); ++i) {
        EXPECT_FALSE(rows[i].heading);
        EXPECT_FALSE(rows[i].value.empty()) << rows[i].label;
        EXPECT_EQ(rows[i].value.front(), '/');
    }
}

TEST(FontInfoTest, the_texts_of_the_font_file_are_read)
{
    // any font with a name table has at least its full name
    for (const std::string& family : tiny::xdg::font_families()) {
        const std::vector<InfoRow> rows = font_info(family);
        if (find(rows, "About the font") != nullptr) {
            EXPECT_NE(find(rows, "Full name"), nullptr);
            return;
        }
    }
    GTEST_SKIP() << "no font with a name table";
}
