#include <gtest/gtest.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "image.h"

namespace fs = std::filesystem;

namespace {

// 2x2 RGBA: red, green / blue, white with the alpha 128
const unsigned char PNG[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00,
        0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d, 0x24,
        0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63,
        0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x0c, 0x81, 0x34, 0x08, 0x34, 0x00,
        0x00, 0x49, 0x49, 0x09, 0x78, 0x28, 0xa0, 0xdb, 0x77, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

class ImageTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-image" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root);
    }

    void TearDown() override { fs::remove_all(root); }

    std::string write(const std::string& name, const std::string& body) const
    {
        const auto file = root / name;
        std::ofstream(file, std::ios::binary) << body;
        return file.string();
    }

    static std::string svg(int width, int height)
    {
        const std::string w = std::to_string(width);
        const std::string h = std::to_string(height);
        return R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" + w +
               R"(" height=")" + h + R"("><rect width=")" + w +
               R"(" height=")" + h + R"(" fill="#ff0000"/></svg>)";
    }

    fs::path root;
};

} // namespace

TEST_F(ImageTest, png_keeps_its_size_and_is_premultiplied)
{
    const auto file =
            write("a.png",
                  std::string(reinterpret_cast<const char*>(PNG), sizeof PNG));
    const auto image = tiny::load_image(file, 48);

    if (!image) {
        FAIL() << "the image was not loaded";
    }
    EXPECT_EQ(image->width, 2);
    EXPECT_EQ(image->height, 2);
    EXPECT_EQ(image->at(0, 0), 0xffff0000u);
    EXPECT_EQ(image->at(1, 0), 0xff00ff00u);
    EXPECT_EQ(image->at(0, 1), 0xff0000ffu);
    EXPECT_EQ(image->at(1, 1), 0x80808080u);
    EXPECT_EQ(image->pixels.size(), 4u);
}

TEST_F(ImageTest, extension_is_case_insensitive)
{
    const auto file =
            write("A.PNG",
                  std::string(reinterpret_cast<const char*>(PNG), sizeof PNG));
    EXPECT_TRUE(tiny::load_image(file, 16));
}

TEST_F(ImageTest, svg_is_rasterized_up_to_the_size)
{
    const auto image = tiny::load_image(write("a.svg", svg(16, 16)), 64);

    if (!image) {
        FAIL() << "the image was not loaded";
    }
    EXPECT_EQ(image->width, 64);
    EXPECT_EQ(image->height, 64);
    EXPECT_EQ(image->at(0, 0), 0xffff0000u);
    EXPECT_EQ(image->at(32, 32), 0xffff0000u);
    EXPECT_EQ(image->at(63, 63), 0xffff0000u);
}

TEST_F(ImageTest, svg_is_rasterized_down_to_the_size)
{
    const auto image = tiny::load_image(write("a.svg", svg(1000, 1000)), 16);

    if (!image) {
        FAIL() << "the image was not loaded";
    }
    EXPECT_EQ(image->width, 16);
    EXPECT_EQ(image->height, 16);
    EXPECT_EQ(image->at(8, 8), 0xffff0000u);
}

TEST_F(ImageTest, svg_keeps_the_aspect_ratio)
{
    const auto wide = tiny::load_image(write("w.svg", svg(40, 20)), 32);
    const auto tall = tiny::load_image(write("t.svg", svg(20, 40)), 32);

    if (!wide) {
        FAIL() << "the image was not loaded";
    }
    if (!tall) {
        FAIL() << "the image was not loaded";
    }
    EXPECT_EQ(wide->width, 32);
    EXPECT_EQ(wide->height, 16);
    EXPECT_EQ(tall->width, 16);
    EXPECT_EQ(tall->height, 32);
}

TEST_F(ImageTest, transparent_part_of_an_svg_is_empty)
{
    const auto image = tiny::load_image(
            write("a.svg", R"(<svg xmlns="http://www.w3.org/2000/svg"
                      width="10" height="10">
                      <rect width="5" height="10" fill="#00ff00"/></svg>)"),
            20);

    if (!image) {
        FAIL() << "the image was not loaded";
    }
    EXPECT_EQ(image->at(2, 10), 0xff00ff00u);
    EXPECT_EQ(image->at(18, 10), 0u);
}

TEST_F(ImageTest, xpm_keeps_its_size)
{
    const auto image = tiny::load_image(
            write("a.xpm", "/* XPM */\nstatic char * a[] = {\n\"2 1 1 1\",\n"
                           "\"a c #00ff00\",\n\"aa\"};\n"),
            48);

    if (!image) {
        FAIL() << "the image was not loaded";
    }
    EXPECT_EQ(image->width, 2);
    EXPECT_EQ(image->height, 1);
    EXPECT_EQ(image->at(1, 0), 0xff00ff00u);
}

TEST_F(ImageTest, broken_and_unsupported_files)
{
    EXPECT_FALSE(tiny::load_image((root / "missing.png").string(), 16));
    EXPECT_FALSE(tiny::load_image(write("bad.png", "not a png"), 16));
    EXPECT_FALSE(tiny::load_image(write("bad.svg", "not an svg"), 16));
    EXPECT_FALSE(tiny::load_image(write("empty.svg", "<svg/>"), 16));
    EXPECT_FALSE(tiny::load_image(write("a.xpm", "/* XPM */"), 16));
    EXPECT_FALSE(tiny::load_image(write("b.ico", "x"), 16));
    EXPECT_FALSE(tiny::load_image(write("noext", "x"), 16));
}

TEST_F(ImageTest, size_out_of_range)
{
    const auto file = write("a.svg", svg(16, 16));
    EXPECT_FALSE(tiny::load_image(file, 0));
    EXPECT_FALSE(tiny::load_image(file, -1));
    EXPECT_FALSE(tiny::load_image(file, 4097));
    EXPECT_TRUE(tiny::load_image(file, 1));
}
