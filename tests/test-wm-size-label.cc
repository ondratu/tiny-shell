#include "helpers.h"

#include "wm_size_label.h"

class SizeLabelTest : public ::testing::Test {
  protected:
    const WidgetFont font;
    wm::SizeLabel label{1, 1};

    void SetUp() override
    {
        label.realize(root_window(), 0, 0);
        label.set_events();
        label.set_area(400, 300, 25);
        label.map();
    }

    // Rows (first and last) of the label that contain a drawn pixel
    std::pair<int, int> drawn_rows()
    {
        const XWindowAttributes attrs = attributes_of(label);
        const Image img(label.get_window(), attrs.width, attrs.height);
        int first = -1;
        int last = -1;
        for (int y = 0; y < attrs.height; ++y) {
            for (int x = 0; x < attrs.width; ++x) {
                if (img.at(x, y) != tiny::theme.widget.normal_bg) {
                    first = first < 0 ? y : first;
                    last = y;
                }
            }
        }
        return {first, last};
    }
};

TEST_F(SizeLabelTest, text_is_kept)
{
    label.set_text("80 × 24");

    EXPECT_EQ(label.get_text(), "80 × 24");
}

TEST_F(SizeLabelTest, box_is_centered_in_the_area)
{
    label.set_text("80 × 24");

    const XWindowAttributes attrs = attributes_of(label);
    EXPECT_NEAR(attrs.x + attrs.width / 2.0, 200, 1);
    EXPECT_NEAR(attrs.y + attrs.height / 2.0, 25 + 150, 1);
}

TEST_F(SizeLabelTest, longer_text_makes_a_wider_box)
{
    label.set_text("8");
    const int narrow = attributes_of(label).width;

    label.set_text("80 × 24");

    EXPECT_GT(attributes_of(label).width, narrow);
}

TEST_F(SizeLabelTest, text_sits_on_the_same_baseline_whatever_the_glyphs)
{
    // "." is low and "H" is tall, but both stand on the baseline
    label.set_text("H");
    send(label, Expose);
    const int tall_bottom = drawn_rows().second;
    const int tall_top = drawn_rows().first;

    label.set_text(".");
    send(label, Expose);

    EXPECT_EQ(drawn_rows().second, tall_bottom);
    EXPECT_GT(drawn_rows().first, tall_top);
}

TEST_F(SizeLabelTest, same_text_does_not_draw_again)
{
    label.set_text("80 × 24");
    send(label, Expose);
    GC gc = XCreateGC(tiny::get_display(), label.get_window(), 0, nullptr);
    XSetForeground(tiny::get_display(), gc, 0xFF0000);
    XFillRectangle(tiny::get_display(), label.get_window(), gc, 1, 1, 2, 2);
    XFreeGC(tiny::get_display(), gc);
    const XWindowAttributes before = attributes_of(label);

    label.set_text("80 × 24"); // e.g. the next motion event of a drag

    const Image img(label.get_window(), before.width, before.height);
    EXPECT_EQ(img.at(1, 1), 0xFF0000u);
}
