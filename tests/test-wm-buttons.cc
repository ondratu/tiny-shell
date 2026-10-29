#include "helpers.h"

#include "wm_buttons.h"

class WmButtonTest: public ::testing::Test {
  protected:
    uint32_t bg() const { return tiny::theme.wm_button.normal_bg; }
    uint32_t fg() const { return tiny::theme.wm_button.normal_fg; }
    uint32_t icon() const { return tiny::theme.wm_win_icon; }

    Image image_of(const tiny::Widget &w)
    { return Image(w.get_window(), w.get_width(), w.get_height()); }
};

TEST_F(WmButtonTest, buttons_are_square_icons)
{
    wm::CloseButton close;
    wm::MaximizeButton maximize;
    wm::MinimizeButton minimize;

    for (tiny::Widget *b: {(tiny::Widget*)&close, (tiny::Widget*)&maximize,
            (tiny::Widget*)&minimize})
    {
        EXPECT_EQ(b->get_width(), icon());
        EXPECT_EQ(b->get_height(), icon());
        EXPECT_EQ(&b->get_style(), &tiny::theme.wm_button);
    }
}

TEST_F(WmButtonTest, expose_draws_cross_on_close_button)
{
    wm::CloseButton b;
    b.realize(root_window(), 0, 0);
    b.map();

    send(b, Expose);

    Image img = image_of(b);
    EXPECT_EQ(img.at(0, 0), bg());
    EXPECT_GT(img.count_not(bg(), icon()), 0u);
    EXPECT_EQ(img.at(icon() / 2, icon() / 2), fg());    // lines cross here
}

TEST_F(WmButtonTest, expose_draws_line_on_minimize_button)
{
    wm::MinimizeButton b;
    b.realize(root_window(), 0, 0);
    b.map();

    send(b, Expose);

    Image img = image_of(b);
    unsigned int drawn = 0;
    for (unsigned int y = 0; y < icon(); ++y) {
        for (unsigned int x = 0; x < icon(); ++x) {
            if (img.at(x, y) != bg()) {
                ++drawn;
                // a short horizontal line below the middle
                EXPECT_GE(y, icon() / 2u + 1) << x << "," << y;
                EXPECT_LE(y, icon() / 2u + 5) << x << "," << y;
            }
        }
    }
    EXPECT_GT(drawn, 0u);
}

TEST_F(WmButtonTest, expose_draws_square_on_maximize_button)
{
    wm::MaximizeButton b;
    b.realize(root_window(), 0, 0);
    b.map();

    send(b, Expose);

    Image img = image_of(b);
    EXPECT_GT(img.count_not(bg(), icon()), 0u);
    EXPECT_EQ(img.at(icon() / 2, icon() / 2 - 3), fg());    // top edge
    EXPECT_EQ(img.at(icon() / 2, icon() / 2), bg());        // hollow
}

TEST_F(WmButtonTest, maximize_button_restore_changes_the_icon)
{
    wm::MaximizeButton b;
    b.realize(root_window(), 0, 0);
    b.map();
    send(b, Expose);
    Image maximize = image_of(b);

    b.set_restore(true);
    Image restore = image_of(b);
    EXPECT_FALSE(maximize == restore);
    EXPECT_GT(restore.count_not(bg(), icon()), 0u);

    b.set_restore(false);
    EXPECT_TRUE(maximize == image_of(b));
}

TEST_F(WmButtonTest, hover_inverts_colors)
{
    wm::CloseButton b;
    b.realize(root_window(), 0, 0);
    b.map();
    send(b, Expose);
    EXPECT_EQ(image_of(b).at(0, 0), bg());

    send(b, EnterNotify);
    EXPECT_EQ(image_of(b).at(0, 0), tiny::theme.wm_button.hover_bg);
    EXPECT_EQ(image_of(b).at(icon() / 2, icon() / 2),
            tiny::theme.wm_button.hover_fg);

    send(b, LeaveNotify);
    EXPECT_EQ(image_of(b).at(0, 0), bg());
}

TEST_F(WmButtonTest, enter_and_leave_signals_stay_free_for_users)
{
    wm::CloseButton b;
    Recorder enter, leave;
    b.on_enter.connect(&enter, enter.slot());
    b.on_leave.connect(&leave, leave.slot());
    b.realize(root_window(), 0, 0);
    b.map();
    send(b, Expose);

    send(b, EnterNotify);
    EXPECT_EQ(enter.calls, 1);
    EXPECT_EQ(image_of(b).at(0, 0), tiny::theme.wm_button.hover_bg);

    send(b, LeaveNotify);
    EXPECT_EQ(leave.calls, 1);
    EXPECT_EQ(image_of(b).at(0, 0), bg());
}

TEST_F(WmButtonTest, click_emits_signal_while_hovering)
{
    wm::MinimizeButton b;
    Recorder click;
    b.on_click.connect(&click, click.slot());
    b.realize(root_window(), 0, 0);
    b.map();

    send(b, ButtonRelease);
    EXPECT_EQ(click.calls, 0);

    send(b, EnterNotify);
    send(b, ButtonRelease);
    EXPECT_EQ(click.calls, 1);
}

TEST_F(WmButtonTest, destructor_disconnects_handlers)
{
    ::Window window;
    {
        wm::CloseButton b;
        b.realize(root_window(), 0, 0);
        window = b.get_window();
        EXPECT_EQ(tiny::handlers.count({Expose, window}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({Expose, window}), 0u);
    EXPECT_EQ(tiny::handlers.count({EnterNotify, window}), 0u);
}

TEST_F(WmButtonTest, delete_through_base_pointer)
{
    tiny::Widget *close = new wm::CloseButton;
    tiny::Widget *maximize = new wm::MaximizeButton;
    tiny::Widget *minimize = new wm::MinimizeButton;

    delete close;
    delete maximize;
    delete minimize;
}

TEST_F(WmButtonTest, destroying_unrealized_button_causes_no_x_error)
{
    XErrors x;
    {
        wm::CloseButton close;
        wm::MaximizeButton maximize;
        wm::MinimizeButton minimize;
    }

    EXPECT_EQ(x.errors(), 0);
}

// ---- scaling (TINY_SCALE_FACTOR) ----

class ScaledWmButtonTest: public WmButtonTest {
  protected:
    void SetUp() override
    { tiny::theme.scale(2); }

    void TearDown() override
    { tiny::theme.scale(1); }

    unsigned int mid() const { return icon() / 2; }
};

TEST_F(ScaledWmButtonTest, close_cross_is_scaled)
{
    wm::CloseButton b;
    b.realize(root_window(), 0, 0);
    b.map();

    send(b, Expose);

    ASSERT_EQ(icon(), 42u);
    Image img = image_of(b);
    // beyond the reach of the unscaled cross (3.5 px from the middle)
    EXPECT_EQ(img.at(mid() + 6, mid() + 6), fg());
    EXPECT_EQ(img.at(mid() - 6, mid() + 6), fg());
}

TEST_F(ScaledWmButtonTest, minimize_line_is_scaled)
{
    wm::MinimizeButton b;
    b.realize(root_window(), 0, 0);
    b.map();

    send(b, Expose);

    Image img = image_of(b);
    EXPECT_EQ(img.at(mid() + 7, mid() + 7), fg());
    EXPECT_EQ(img.at(mid() - 7, mid() + 7), fg());
}

TEST_F(ScaledWmButtonTest, maximize_square_is_scaled)
{
    wm::MaximizeButton b;
    b.realize(root_window(), 0, 0);
    b.map();

    send(b, Expose);

    Image img = image_of(b);
    EXPECT_EQ(img.at(mid(), mid() - 6), fg());
    EXPECT_EQ(img.at(mid() - 6, mid()), fg());
    EXPECT_EQ(img.at(mid(), mid()), bg());
}
