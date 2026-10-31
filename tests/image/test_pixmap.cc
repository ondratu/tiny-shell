// gtest must be included before Xlib: X.h defines the None macro
#include <gtest/gtest.h>

#include "display.h"
#include "image/pixmap.h"
#include "x_util.h"

TEST(CreatePixmapTest, puts_the_pixels_to_the_server)
{
    ::Display* display = tiny::get_display();
    const ::Window root = XDefaultRootWindow(display);

    tiny::image::Image image;
    image.width = 3;
    image.height = 2;
    image.pixels = {0xFFFF0000U, 0xFF00FF00U, 0xFF0000FFU,
                    0x80404040U, 0x00000000U, 0xFFFFFFFFU};

    const Pixmap pixmap = tiny::image::create_pixmap(display, root, image);
    if (pixmap == None) {
        GTEST_SKIP() << "no ARGB32 format";
    }

    ::Window r;
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    unsigned int border;
    unsigned int depth;
    XGetGeometry(display, pixmap, &r, &x, &y, &w, &h, &border, &depth);
    EXPECT_EQ(w, 3u);
    EXPECT_EQ(h, 2u);
    EXPECT_EQ(depth, 32u);

    XImage* back = XGetImage(display, pixmap, 0, 0, 3, 2, AllPlanes, ZPixmap);
    ASSERT_NE(back, nullptr);
    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 3; ++i) {
            EXPECT_EQ(static_cast<uint32_t>(XGetPixel(back, i, j)),
                      image.at(i, j));
        }
    }
    XDestroyImage(back);

    const Picture picture = tiny::image::create_argb_picture(display, pixmap);
    EXPECT_NE(picture, None);
    XRenderFreePicture(display, picture);
    XFreePixmap(display, pixmap);
}

TEST(CreatePixmapTest, refuses_an_empty_or_broken_picture)
{
    ::Display* display = tiny::get_display();
    const ::Window root = XDefaultRootWindow(display);

    tiny::image::Image image;
    EXPECT_EQ(tiny::image::create_pixmap(display, root, image), None);

    image.width = 2;
    image.height = 2;
    image.pixels.assign(3, 0);
    EXPECT_EQ(tiny::image::create_pixmap(display, root, image), None);

    EXPECT_EQ(tiny::image::create_argb_picture(display, None), None);
}

TEST(CreatePixmapTest, refuses_a_picture_over_the_limit_of_the_protocol)
{
    ::Display* display = tiny::get_display();
    tiny::image::Image image;
    image.width = 70000;
    image.height = 1;
    image.pixels.assign(70000, 0);
    EXPECT_EQ(tiny::image::create_pixmap(display, XDefaultRootWindow(display),
                                         image),
              None);
}

TEST(CreatePixmapTest, picture_is_not_made_for_other_depth_or_missing_pixmap)
{
    ::Display* display = tiny::get_display();
    const ::Window root = XDefaultRootWindow(display);

    const Pixmap plain = XCreatePixmap(display, root, 4, 4, 1);
    EXPECT_EQ(tiny::image::create_argb_picture(display, plain), None);
    XFreePixmap(display, plain);

    // a pixmap that does not exist is an X error, not an exit
    EXPECT_EQ(tiny::image::create_argb_picture(display, 0x7fffff), None);
}

// ---- resize_pixmap ----

class ResizePixmapTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        display = tiny::get_display();
        root = XDefaultRootWindow(display);
    }

    Pixmap filled(unsigned int w, unsigned int h, unsigned int depth,
                  unsigned long color)
    {
        const Pixmap pm = XCreatePixmap(display, root, w, h, depth);
        GC gc = XCreateGC(display, pm, 0, nullptr);
        XSetForeground(display, gc, color);
        XFillRectangle(display, pm, gc, 0, 0, w, h);
        XFreeGC(display, gc);
        return pm;
    }

    void geometry(Pixmap pm, unsigned int& w, unsigned int& h,
                  unsigned int& depth)
    {
        ::Window r;
        int x;
        int y;
        unsigned int border;
        XSync(display, False);
        XGetGeometry(display, pm, &r, &x, &y, &w, &h, &border, &depth);
    }

    ::Display* display;
    ::Window root;
};

TEST_F(ResizePixmapTest, resize_creates_pixmap_of_target_size)
{
    const int depth = DefaultDepth(display, 0);
    const Pixmap origin = filled(100, 100, tiny::x_uint(depth), 0xFF0000);

    const Pixmap target = tiny::image::resize_pixmap(
            display, root, origin, 100, 100, 50, 50, tiny::x_uint(depth));

    unsigned int w;
    unsigned int h;
    unsigned int d;
    geometry(target, w, h, d);
    EXPECT_EQ(w, 50u);
    EXPECT_EQ(h, 50u);
    EXPECT_EQ(d, static_cast<unsigned int>(depth));
    XFreePixmap(display, origin);
    XFreePixmap(display, target);
}

TEST_F(ResizePixmapTest, resize_keeps_color)
{
    const int depth = DefaultDepth(display, 0);
    const Pixmap origin = filled(100, 100, tiny::x_uint(depth), 0xFF0000);

    const Pixmap target = tiny::image::resize_pixmap(
            display, root, origin, 100, 100, 50, 50, tiny::x_uint(depth));

    XSync(display, False);
    XImage* img = XGetImage(display, target, 10, 10, 1, 1, AllPlanes, ZPixmap);
    EXPECT_EQ(XGetPixel(img, 0, 0) & 0xFFFFFF, 0xFF0000u);
    XDestroyImage(img);
    XFreePixmap(display, origin);
    XFreePixmap(display, target);
}

TEST_F(ResizePixmapTest, resize_monochrome_pixmap)
{
    const Pixmap origin = filled(64, 32, 1, 1);

    const Pixmap target =
            tiny::image::resize_pixmap(display, root, origin, 64, 32, 16, 8, 1);

    unsigned int w;
    unsigned int h;
    unsigned int d;
    geometry(target, w, h, d);
    EXPECT_EQ(w, 16u);
    EXPECT_EQ(h, 8u);
    EXPECT_EQ(d, 1u);
    XFreePixmap(display, origin);
    XFreePixmap(display, target);
}
