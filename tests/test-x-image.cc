// gtest must be included before Xlib: X.h defines the None macro
#include <gtest/gtest.h>

#include "display.h"
#include "x_image.h"

TEST(ImagePixmap, puts_the_pixels_to_the_server)
{
    ::Display* display = tiny::get_display();
    const ::Window root = XDefaultRootWindow(display);

    tiny::Image image;
    image.width = 3;
    image.height = 2;
    image.pixels = {0xFFFF0000U, 0xFF00FF00U, 0xFF0000FFU,
                    0x80404040U, 0x00000000U, 0xFFFFFFFFU};

    const Pixmap pixmap = tiny::image_to_pixmap(display, root, image);
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

    const Picture picture = tiny::argb_picture(display, pixmap);
    EXPECT_NE(picture, None);
    XRenderFreePicture(display, picture);
    XFreePixmap(display, pixmap);
}

TEST(ImagePixmap, refuses_an_empty_or_broken_picture)
{
    ::Display* display = tiny::get_display();
    const ::Window root = XDefaultRootWindow(display);

    tiny::Image image;
    EXPECT_EQ(tiny::image_to_pixmap(display, root, image), None);

    image.width = 2;
    image.height = 2;
    image.pixels.assign(3, 0);
    EXPECT_EQ(tiny::image_to_pixmap(display, root, image), None);

    EXPECT_EQ(tiny::argb_picture(display, None), None);
}

TEST(ImagePixmap, refuses_a_picture_over_the_limit_of_the_protocol)
{
    ::Display* display = tiny::get_display();
    tiny::Image image;
    image.width = 70000;
    image.height = 1;
    image.pixels.assign(70000, 0);
    EXPECT_EQ(
            tiny::image_to_pixmap(display, XDefaultRootWindow(display), image),
            None);
}

TEST(ImagePixmap, picture_is_not_made_for_other_depth_or_missing_pixmap)
{
    ::Display* display = tiny::get_display();
    const ::Window root = XDefaultRootWindow(display);

    const Pixmap plain = XCreatePixmap(display, root, 4, 4, 1);
    EXPECT_EQ(tiny::argb_picture(display, plain), None);
    XFreePixmap(display, plain);

    // a pixmap that does not exist is an X error, not an exit
    EXPECT_EQ(tiny::argb_picture(display, 0x7fffff), None);
}
