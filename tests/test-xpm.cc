#include <gtest/gtest.h>

#include "xpm.h"

namespace {

const char* const SAMPLE = R"(/* XPM */
static char * sample_xpm[] = {
/* columns rows colors chars-per-pixel */
"3 2 4 1 0 0",
"  c None",
". c #FF0000",
"# c #0000FF s sym",
"x m black c green",
/* pixels */
".# ",
"x. "
};
)";

} // namespace

TEST(Xpm, parses_colors_and_transparency)
{
    const auto image = tiny::parse_xpm(SAMPLE);

    if (!image) {
        FAIL() << "the picture was not parsed";
    }
    EXPECT_EQ(image->width, 3);
    EXPECT_EQ(image->height, 2);
    EXPECT_EQ(image->at(0, 0), 0xffff0000u);
    EXPECT_EQ(image->at(1, 0), 0xff0000ffu);
    EXPECT_EQ(image->at(2, 0), 0u);
    EXPECT_EQ(image->at(0, 1), 0xff00ff00u); // c wins over m
    EXPECT_EQ(image->at(2, 1), 0u);
}

TEST(Xpm, two_characters_per_pixel_and_long_hex_colors)
{
    const auto image = tiny::parse_xpm(R"(/* XPM */
static char * a[] = {
"2 1 2 2",
"aa c #fff000000000",
"bb c #0f0",
"aabb"
};)");

    if (!image) {
        FAIL() << "the picture was not parsed";
    }
    EXPECT_EQ(image->at(0, 0), 0xffff0000u);
    EXPECT_EQ(image->at(1, 0), 0xff00ff00u);
}

TEST(Xpm, color_names_and_gray_levels)
{
    const auto image = tiny::parse_xpm(R"(/* XPM */
static char * a[] = {
"4 1 4 1",
"a c Dark Green",
"b c gray50",
"c c unknowncolor",
"d c #12",
"abcd"
};)");

    if (!image) {
        FAIL() << "the picture was not parsed";
    }
    EXPECT_EQ(image->at(0, 0), 0xff006400u);
    EXPECT_EQ(image->at(1, 0), 0xff808080u);
    EXPECT_EQ(image->at(2, 0), 0u);
    EXPECT_EQ(image->at(3, 0), 0u);
}

TEST(Xpm, escaped_characters_in_strings)
{
    const auto image = tiny::parse_xpm(R"(/* XPM */
static char * a[] = {
"1 1 1 1",
"\" c #ffffff",
"\""
};)");

    if (!image) {
        FAIL() << "the picture was not parsed";
    }
    EXPECT_EQ(image->at(0, 0), 0xffffffffu);
}

TEST(Xpm, broken_files)
{
    EXPECT_FALSE(tiny::parse_xpm(""));
    EXPECT_FALSE(tiny::parse_xpm("not a picture"));
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */"));
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */ \"1 1\""));
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */ \"0 1 1 1\",\"a c red\",\"a\""));
    EXPECT_FALSE(
            tiny::parse_xpm("/* XPM */ \"99999 1 1 1\",\"a c red\",\"a\""));
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */ \"1 1 1 9\",\"a c red\",\"a\""));
    // too few rows, a short row, a pixel without a color
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */ \"1 2 1 1\",\"a c red\",\"a\""));
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */ \"2 1 1 1\",\"a c red\",\"a\""));
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */ \"1 1 1 1\",\"a c red\",\"b\""));
    EXPECT_FALSE(tiny::parse_xpm("/* XPM */ /* open"));
}
