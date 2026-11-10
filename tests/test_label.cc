#include "helpers.h"

#include "label.h"

class LabelTest : public ::testing::Test {
  protected:
    WidgetFont font;
    tiny::Label label{100, 30, "Run"};

    void SetUp() override
    {
        label.realize(root_window(), 0, 0);
        label.set_events();
        label.map();
    }

    unsigned int drawn()
    {
        return Image(label.get_window(), 100, 30)
                .count_not(tiny::theme.widget.normal_bg, 30);
    }
};

TEST_F(LabelTest, label_keeps_its_text)
{
    EXPECT_EQ(label.get_text(), "Run");

    label.set_text("Other");

    EXPECT_EQ(label.get_text(), "Other");
}

TEST_F(LabelTest, label_has_no_border)
{
    EXPECT_EQ(label.get_border(), 0);
    EXPECT_EQ(attributes_of(label).border_width, 0);
}

TEST_F(LabelTest, expose_draws_the_text)
{
    send(label, Expose);

    EXPECT_GT(drawn(), 0u);
}

TEST_F(LabelTest, text_is_aligned_to_the_left)
{
    send(label, Expose);

    const Image img = Image(label.get_window(), 100, 30);
    bool left = false;
    bool right = false;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 100; ++x) {
            if (img.at(x, y) != tiny::theme.widget.normal_bg) {
                (x < 50 ? left : right) = true;
            }
        }
    }
    EXPECT_TRUE(left);
    EXPECT_FALSE(right);
}

TEST_F(LabelTest, empty_text_draws_nothing)
{
    label.set_text("");

    EXPECT_EQ(drawn(), 0u);
}

TEST_F(LabelTest, set_text_redraws_the_label)
{
    label.set_text("");
    ASSERT_EQ(drawn(), 0u);

    label.set_text("Run a command");

    EXPECT_GT(drawn(), 0u);
}

TEST(LabelLifeTest, destructor_disconnects_the_expose_handler)
{
    const WidgetFont font;
    ::Window window;
    {
        tiny::Label label(100, 30, "x");
        label.realize(root_window(), 0, 0);
        label.set_events();
        window = label.get_window();
        ASSERT_TRUE(tiny::handlers.count({Expose, window}));
    }

    EXPECT_FALSE(tiny::handlers.count({Expose, window}));
}

TEST_F(LabelTest, it_says_how_big_its_text_is)
{
    EXPECT_GT(label.text_width(), 0);
    EXPECT_GT(label.natural_height(), 0);
    const int before = label.text_width();

    label.set_text("A much longer text than before");

    EXPECT_GT(label.text_width(), before);
}

TEST_F(LabelTest, an_empty_text_has_no_width_but_a_height)
{
    label.set_text("");

    EXPECT_EQ(label.text_width(), 0);
    EXPECT_GT(label.natural_height(), 0);
}

TEST_F(LabelTest, a_bigger_font_makes_the_text_bigger)
{
    const int width = label.text_width();
    const int height = label.natural_height();

    label.set_font_scale(2);

    EXPECT_GT(label.text_width(), width * 3 / 2);
    EXPECT_GT(label.natural_height(), height);
}

TEST_F(LabelTest, the_scale_one_goes_back_to_the_font_of_the_style)
{
    const int width = label.text_width();
    label.set_font_scale(2);

    label.set_font_scale(1);

    EXPECT_EQ(label.text_width(), width);
}

TEST_F(LabelTest, a_bigger_font_is_drawn)
{
    label.set_font_scale(2);

    send(label, Expose);

    EXPECT_GT(drawn(), 0u);
}

TEST_F(LabelTest, text_in_the_middle_leaves_the_left_edge_empty)
{
    label.set_align(tiny::Label::Align::Center);

    send(label, Expose);

    const Image img = Image(label.get_window(), 100, 30);
    // the text is about 30 px wide: the first 20 columns are the background
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 30; ++y) {
            ASSERT_EQ(img.at(x, y), tiny::theme.widget.normal_bg)
                    << x << "," << y;
        }
    }
    EXPECT_GT(drawn(), 0u);
}
