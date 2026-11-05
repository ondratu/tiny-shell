#include "helpers.h"

#include <X11/Xatom.h>

#include <filesystem>
#include <fstream>

#include "app/wallpaper.h"

using tiny::app::apply_wallpaper;
using tiny::app::expand_home;
using tiny::app::parse_wallpaper_args;
using tiny::app::set_root_pixmap;
using tiny::app::wallpaper_fit;
using tiny::image::Fit;
using Pic = tiny::image::Image;

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

Pic solid(int width, int height, uint32_t pixel)
{
    Pic image;
    image.width = width;
    image.height = height;
    image.pixels.assign(
            static_cast<size_t>(width) * static_cast<size_t>(height), pixel);
    return image;
}

//! The pixmap that a property of the root window names, None when none
Pixmap property(const char* name)
{
    ::Display* display = tiny::get_display();
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    Pixmap pixmap = None;
    if (XGetWindowProperty(display, root_window(),
                           XInternAtom(display, name, False), 0, 1, False,
                           XA_PIXMAP, &type, &format, &count, &after,
                           &data) == Success &&
        data != nullptr) {
        if (type == XA_PIXMAP && format == 32 && count == 1) {
            pixmap = *reinterpret_cast<Pixmap*>(data);
        }
        XFree(data);
    }
    return pixmap;
}

int quiet(::Display*, XErrorEvent*)
{
    return 0;
}

//! Whether the server still has the pixmap
bool exists(::Display* display, Pixmap pixmap)
{
    const auto previous = XSetErrorHandler(&quiet);
    ::Window root = 0;
    int x = 0;
    int y = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int border = 0;
    unsigned int depth = 0;
    const Status ok = XGetGeometry(display, pixmap, &root, &x, &y, &width,
                                   &height, &border, &depth);
    XSync(display, False);
    XSetErrorHandler(previous);
    return ok != 0;
}

class WallpaperTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        clear();
        dir = std::filesystem::temp_directory_path() /
              ("tiny-wallpaper" + std::to_string(getpid()));
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
    }

    void TearDown() override
    {
        clear();
        std::filesystem::remove_all(dir);
    }

    //! The properties and the pixmap of the test, so that the next test has
    //! none (a pixmap of this connection must not be left to the next call)
    static void clear()
    {
        ::Display* display = tiny::get_display();
        if (const Pixmap pixmap = property("_XROOTPMAP_ID")) {
            XFreePixmap(display, pixmap);
        }
        XDeleteProperty(display, root_window(),
                        XInternAtom(display, "_XROOTPMAP_ID", False));
        XDeleteProperty(display, root_window(),
                        XInternAtom(display, "ESETROOT_PMAP_ID", False));
        XSetWindowBackground(display, root_window(), 0);
        XClearWindow(display, root_window());
        XSync(display, False);
    }

    std::string write_png() const
    {
        const auto file = dir / "a.png";
        std::ofstream(file, std::ios::binary)
                << std::string(reinterpret_cast<const char*>(PNG), sizeof PNG);
        return file.string();
    }

    std::filesystem::path dir;
};

} // namespace

TEST(WallpaperArgsTest, the_mode_decides_how_the_photo_fits)
{
    EXPECT_EQ(wallpaper_fit(""), Fit::Fill);
    EXPECT_EQ(wallpaper_fit("fill"), Fit::Fill);
    EXPECT_EQ(wallpaper_fit("center"), Fit::Center);
    EXPECT_EQ(wallpaper_fit("tile"), Fit::Tile);
    EXPECT_EQ(wallpaper_fit("bogus"), Fit::Fill);
}

TEST(WallpaperArgsTest, the_home_directory_is_expanded)
{
    EXPECT_EQ(expand_home("~/a.jpg", "/home/me"), "/home/me/a.jpg");
    EXPECT_EQ(expand_home("~", "/home/me"), "/home/me");
    EXPECT_EQ(expand_home("~other/a.jpg", "/home/me"), "~other/a.jpg");
    EXPECT_EQ(expand_home("/a/~/b", "/home/me"), "/a/~/b");
    EXPECT_EQ(expand_home("a.jpg", "/home/me"), "a.jpg");
    EXPECT_EQ(expand_home("~/a.jpg", nullptr), "~/a.jpg");
    EXPECT_EQ(expand_home("~/a.jpg", ""), "~/a.jpg");
    EXPECT_EQ(expand_home("", "/home/me"), "");
}

TEST(WallpaperArgsTest, arguments)
{
    auto r = parse_wallpaper_args({});
    EXPECT_TRUE(r.file.empty());
    EXPECT_TRUE(r.mode.empty());
    EXPECT_TRUE(r.error.empty());
    EXPECT_FALSE(r.help);

    r = parse_wallpaper_args({"-m", "tile", "a.png"});
    EXPECT_EQ(r.file, "a.png");
    EXPECT_EQ(r.mode, "tile");
    EXPECT_TRUE(r.error.empty());

    r = parse_wallpaper_args({"--mode", "fill", "-"});
    EXPECT_TRUE(r.mode.empty()); // fill is the default
    EXPECT_EQ(r.file, "-");

    EXPECT_TRUE(parse_wallpaper_args({"-h"}).help);
    EXPECT_TRUE(parse_wallpaper_args({"--help"}).help);
}

TEST(WallpaperArgsTest, wrong_arguments_are_told)
{
    EXPECT_FALSE(parse_wallpaper_args({"-m"}).error.empty());
    EXPECT_FALSE(parse_wallpaper_args({"-m", "stretch"}).error.empty());
    EXPECT_FALSE(parse_wallpaper_args({"--bad"}).error.empty());
    EXPECT_FALSE(parse_wallpaper_args({"a.png", "b.png"}).error.empty());
}

TEST(WallpaperArgsTest, the_usage_names_the_program_and_the_modes)
{
    const std::string usage = tiny::app::wallpaper_usage("tiny-wallpaper");
    EXPECT_NE(usage.find("Usage: tiny-wallpaper"), std::string::npos);
    EXPECT_NE(usage.find("center"), std::string::npos);
}

TEST_F(WallpaperTest, the_picture_is_the_background_of_the_root_window)
{
    ::Display* display = tiny::get_display();
    ASSERT_TRUE(
            set_root_pixmap(display, root_window(), solid(4, 3, 0xff336699)));

    EXPECT_EQ(::Image(root_window(), 1, 1).at(0, 0), 0x336699u);
    const Pixmap pixmap = property("_XROOTPMAP_ID");
    ASSERT_NE(pixmap, static_cast<Pixmap>(None));
    EXPECT_EQ(property("ESETROOT_PMAP_ID"), pixmap);
    EXPECT_TRUE(exists(display, pixmap));
    ::Window root = 0;
    int x = 0;
    int y = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int border = 0;
    unsigned int depth = 0;
    XGetGeometry(display, pixmap, &root, &x, &y, &width, &height, &border,
                 &depth);
    EXPECT_EQ(width, 4u);
    EXPECT_EQ(height, 3u);
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root_window(), &attrs);
    EXPECT_EQ(static_cast<int>(depth), attrs.depth); // the depth of the root
}

TEST_F(WallpaperTest, an_empty_picture_changes_nothing)
{
    EXPECT_FALSE(set_root_pixmap(tiny::get_display(), root_window(), Pic{}));
    EXPECT_EQ(property("_XROOTPMAP_ID"), static_cast<Pixmap>(None));
}

TEST_F(WallpaperTest, a_picture_too_big_for_a_pixmap_is_refused)
{
    EXPECT_FALSE(set_root_pixmap(tiny::get_display(), root_window(),
                                 solid(65536, 1, 0xff000000)));
    EXPECT_EQ(property("_XROOTPMAP_ID"), static_cast<Pixmap>(None));
}

TEST_F(WallpaperTest, the_pixmap_of_the_previous_wallpaper_is_freed)
{
    // another program set a wallpaper and is gone, with RetainPermanent
    ::Display* other = XOpenDisplay(nullptr);
    ASSERT_NE(other, nullptr);
    XSetCloseDownMode(other, RetainPermanent);
    const Pixmap old = XCreatePixmap(
            other, DefaultRootWindow(other), 2, 2,
            static_cast<unsigned>(DefaultDepth(other, DefaultScreen(other))));
    for (const char* name : {"_XROOTPMAP_ID", "ESETROOT_PMAP_ID"}) {
        XChangeProperty(other, DefaultRootWindow(other),
                        XInternAtom(other, name, False), XA_PIXMAP, 32,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&old), 1);
    }
    XCloseDisplay(other);
    ASSERT_TRUE(exists(tiny::get_display(), old));

    ASSERT_TRUE(set_root_pixmap(tiny::get_display(), root_window(),
                                solid(2, 2, 0xff112233)));

    EXPECT_NE(property("_XROOTPMAP_ID"), old);
    EXPECT_FALSE(exists(tiny::get_display(), old));
}

TEST_F(WallpaperTest, a_pixmap_that_was_not_published_as_a_pair_is_kept)
{
    ::Display* other = XOpenDisplay(nullptr);
    ASSERT_NE(other, nullptr);
    XSetCloseDownMode(other, RetainPermanent);
    const Pixmap old = XCreatePixmap(
            other, DefaultRootWindow(other), 2, 2,
            static_cast<unsigned>(DefaultDepth(other, DefaultScreen(other))));
    // only _XROOTPMAP_ID: it belongs to a program that is still running
    XChangeProperty(other, DefaultRootWindow(other),
                    XInternAtom(other, "_XROOTPMAP_ID", False), XA_PIXMAP, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&old), 1);
    XCloseDisplay(other);

    ASSERT_TRUE(set_root_pixmap(tiny::get_display(), root_window(),
                                solid(2, 2, 0xff112233)));

    EXPECT_TRUE(exists(tiny::get_display(), old));
    XKillClient(tiny::get_display(), old); // not to leave it in the server
    XSync(tiny::get_display(), False);
}

TEST_F(WallpaperTest, a_file_is_fitted_to_the_size_of_the_screen)
{
    ::Display* display = tiny::get_display();
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root_window(), &attrs);
    std::string why;

    ASSERT_TRUE(apply_wallpaper(display, root_window(), write_png(),
                                Fit::Center, &why))
            << why;

    const Pixmap pixmap = property("_XROOTPMAP_ID");
    ::Window root = 0;
    int x = 0;
    int y = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int border = 0;
    unsigned int depth = 0;
    XGetGeometry(display, pixmap, &root, &x, &y, &width, &height, &border,
                 &depth);
    EXPECT_EQ(static_cast<int>(width), attrs.width);
    EXPECT_EQ(static_cast<int>(height), attrs.height);
    // the picture is in the middle, black around it
    const ::Image shot(root_window(), attrs.width, attrs.height);
    EXPECT_EQ(shot.at(0, 0), 0u);
    EXPECT_EQ(shot.at(attrs.width / 2 - 1, attrs.height / 2 - 1), 0xff0000u);
}

TEST_F(WallpaperTest, a_file_that_is_not_a_picture_is_told)
{
    std::string why;
    EXPECT_FALSE(apply_wallpaper(tiny::get_display(), root_window(),
                                 (dir / "missing.jpg").string(), Fit::Fill,
                                 &why));
    EXPECT_NE(why.find("missing.jpg"), std::string::npos);
    EXPECT_EQ(property("_XROOTPMAP_ID"), static_cast<Pixmap>(None));
    EXPECT_FALSE(apply_wallpaper(tiny::get_display(), root_window(),
                                 (dir / "missing.jpg").string(), Fit::Fill,
                                 nullptr)); // the reason is optional
}

// The thumbnail for the settings

TEST_F(WallpaperTest, the_thumbnail_fits_the_box_and_is_opaque)
{
    const auto thumb =
            tiny::app::wallpaper_thumbnail(write_png(), nullptr, 100, 100);

    ASSERT_TRUE(thumb.has_value());
    EXPECT_TRUE(thumb->valid());
    EXPECT_LE(thumb->width, 100);
    EXPECT_LE(thumb->height, 100);
    for (const uint32_t pixel : thumb->pixels) {
        EXPECT_EQ(pixel >> 24, 255u);
    }
}

TEST_F(WallpaperTest, the_thumbnail_is_never_bigger_than_the_photo)
{
    const auto photo = tiny::image::load_photo(write_png());
    ASSERT_TRUE(photo.has_value());

    const auto thumb =
            tiny::app::wallpaper_thumbnail(write_png(), nullptr, 4000, 4000);

    ASSERT_TRUE(thumb.has_value());
    EXPECT_LE(thumb->width, photo->width);
    EXPECT_LE(thumb->height, photo->height);
}

TEST_F(WallpaperTest, the_thumbnail_keeps_the_aspect_ratio_in_a_narrow_box)
{
    const auto photo = tiny::image::load_photo(write_png());
    ASSERT_TRUE(photo.has_value());

    const auto thumb =
            tiny::app::wallpaper_thumbnail(write_png(), nullptr, 1, 1000);

    ASSERT_TRUE(thumb.has_value());
    EXPECT_EQ(thumb->width, 1);
}

TEST_F(WallpaperTest, the_thumbnail_of_a_home_path_is_found)
{
    write_png();

    const auto thumb = tiny::app::wallpaper_thumbnail(
            "~/a.png", dir.string().c_str(), 50, 50);

    EXPECT_TRUE(thumb.has_value());
}

TEST_F(WallpaperTest, there_is_no_thumbnail_of_what_is_not_a_picture)
{
    std::ofstream(dir / "t.txt") << "text";

    EXPECT_FALSE(tiny::app::wallpaper_thumbnail((dir / "t.txt").string(),
                                                nullptr, 50, 50));
    EXPECT_FALSE(tiny::app::wallpaper_thumbnail("/no/such/file.png", nullptr,
                                                50, 50));
    EXPECT_FALSE(tiny::app::wallpaper_thumbnail(write_png(), nullptr, 0, 50));
}
