#include "wm_helpers.h"

#include <vector>

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "wm/dock_backdrop.h"

using tiny::wm::half_bar_shape;
using tiny::wm::make_backdrop;
using tiny::wm::root_pixmap;

namespace {

uint32_t pixel(Pixmap pixmap, int x, int y)
{
    XImage* image =
            XGetImage(tiny::get_display(), pixmap, x, y, 1, 1, ~0UL, ZPixmap);
    const uint32_t value = static_cast<uint32_t>(XGetPixel(image, 0, 0));
    XDestroyImage(image);
    return value & 0xFFFFFF;
}

//! A wallpaper of one color, published as the pixmap of the root window
class Wallpaper {
  public:
    explicit Wallpaper(uint32_t color)
    {
        ::Display* display = tiny::get_display();
        const int screen = XDefaultScreen(display);
        pixmap = XCreatePixmap(
                display, root_window(), 200, 100,
                static_cast<unsigned>(XDefaultDepth(display, screen)));
        GC gc = XCreateGC(display, pixmap, 0, nullptr);
        XSetForeground(display, gc, color);
        XFillRectangle(display, pixmap, gc, 0, 0, 200, 100);
        XFreeGC(display, gc);
        set(pixmap);
    }

    ~Wallpaper()
    {
        XDeleteProperty(tiny::get_display(), root_window(),
                        tiny::get_display()._XROOTPMAP_ID);
        XFreePixmap(tiny::get_display(), pixmap);
        XSync(tiny::get_display(), False);
    }

    Wallpaper(const Wallpaper&) = delete;
    Wallpaper& operator=(const Wallpaper&) = delete;

    static void set(Pixmap value)
    {
        XChangeProperty(tiny::get_display(), root_window(),
                        tiny::get_display()._XROOTPMAP_ID, XA_PIXMAP, 32,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&value), 1);
    }

    Pixmap pixmap;
};

class BackdropTest : public ::testing::Test {
  protected:
    void TearDown() override
    {
        if (made != None) {
            XFreePixmap(tiny::get_display(), made);
        }
    }

    Pixmap make(int bar_top, uint32_t bar, int opacity)
    {
        made = make_backdrop(tiny::get_display(), root_window(),
                             {10, 20, 50, 40}, bar_top, bar, opacity, 0x112233);
        return made;
    }

    Pixmap made = None;
};

TEST_F(BackdropTest, there_is_no_root_pixmap_without_a_wallpaper)
{
    EXPECT_EQ(root_pixmap(tiny::get_display(), root_window()), None);
}

TEST_F(BackdropTest, the_root_pixmap_is_the_one_of_the_property)
{
    const Wallpaper wallpaper(0x204060);

    EXPECT_EQ(root_pixmap(tiny::get_display(), root_window()),
              wallpaper.pixmap);
}

TEST_F(BackdropTest, a_pixmap_that_is_gone_is_not_the_root_pixmap)
{
    ::Display* display = tiny::get_display();
    const Pixmap gone = XCreatePixmap(display, root_window(), 4, 4, 24);
    XFreePixmap(display, gone);
    Wallpaper::set(gone);

    EXPECT_EQ(root_pixmap(display, root_window()), None);
    XDeleteProperty(display, root_window(), tiny::get_display()._XROOTPMAP_ID);
}

TEST_F(BackdropTest, an_opaque_bar_hides_the_wallpaper)
{
    const Wallpaper wallpaper(0x204060);

    ASSERT_NE(make(10, 0xFF0000, 100), None);

    EXPECT_EQ(pixel(made, 5, 20), 0xFF0000u);
}

TEST_F(BackdropTest, above_the_bar_there_is_the_wallpaper)
{
    const Wallpaper wallpaper(0x204060);

    ASSERT_NE(make(10, 0xFF0000, 100), None);

    EXPECT_EQ(pixel(made, 5, 5), 0x204060u);
}

TEST_F(BackdropTest, a_transparent_bar_is_the_wallpaper)
{
    const Wallpaper wallpaper(0x204060);

    ASSERT_NE(make(0, 0xFF0000, 0), None);

    EXPECT_EQ(pixel(made, 5, 20), 0x204060u);
}

TEST_F(BackdropTest, a_half_transparent_bar_mixes_its_color_with_the_wallpaper)
{
    const Wallpaper wallpaper(0x000000);

    ASSERT_NE(make(0, 0xFF0000, 50), None);

    const uint32_t red = (pixel(made, 5, 20) >> 16) & 0xFF;
    EXPECT_NEAR(static_cast<int>(red), 127, 2);
    EXPECT_EQ(pixel(made, 5, 20) & 0xFFFF, 0u);
}

TEST_F(BackdropTest, without_a_wallpaper_the_fallback_is_under_the_bar)
{
    ASSERT_NE(make(10, 0xFF0000, 0), None);

    EXPECT_EQ(pixel(made, 5, 20), 0x112233u);
}

TEST_F(BackdropTest, an_empty_place_has_no_backdrop)
{
    EXPECT_EQ(make_backdrop(tiny::get_display(), root_window(), {0, 0, 0, 5}, 0,
                            0, 100, 0),
              None);
}

TEST(HalfBarShapeTest, the_bar_is_the_lower_half_and_the_parts_follow)
{
    const std::vector<XRectangle> shape =
            half_bar_shape(100, 58, {{5, 5, 48, 53}, {60, 5, 1, -3}});

    ASSERT_EQ(shape.size(), 3u);
    EXPECT_EQ(shape[0].x, 0);
    EXPECT_EQ(shape[0].y, 29);
    EXPECT_EQ(shape[0].width, 100);
    EXPECT_EQ(shape[0].height, 29);
    EXPECT_EQ(shape[1].x, 5);
    EXPECT_EQ(shape[1].height, 53);
    EXPECT_EQ(shape[2].height, 0) << "a negative size is nothing";
}

} // namespace
