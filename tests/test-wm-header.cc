#include "helpers.h"

#include "wm_header.h"

// ---- TitleBox ----

TEST(TitleBoxTest, realize_creates_input_only_window)
{
    wm::TitleBox box(100, 25);

    box.realize(root_window(), 1, 2);

    const XWindowAttributes attrs = attributes_of(box);
    EXPECT_EQ(attrs.c_class, InputOnly);
    EXPECT_EQ(attrs.x, 1);
    EXPECT_EQ(attrs.y, 2);
    EXPECT_EQ(attrs.width, 100);
    EXPECT_EQ(attrs.height, 25);
}

TEST(TitleBoxTest, drag_signals_follow_pointer_events)
{
    wm::TitleBox box(100, 25);
    Recorder begin;
    Recorder end;
    Recorder motion;
    box.on_drag_begin.connect(&begin, begin.slot());
    box.on_drag_end.connect(&end, end.slot());
    box.on_drag_motion.connect(&motion, motion.slot());
    box.realize(root_window(), 0, 0);

    send(box, ButtonPress);
    EXPECT_EQ(begin.calls, 1);
    EXPECT_EQ(begin.last_owner, &box);

    XEvent move = make_event(MotionNotify);
    move.xmotion.window = box.get_window();
    move.xmotion.x_root = 77;
    send(box.get_window(), move);
    EXPECT_EQ(motion.calls, 1);
    EXPECT_EQ(motion.last_event.xmotion.x_root, 77);

    send(box, ButtonRelease);
    EXPECT_EQ(end.calls, 1);
    EXPECT_EQ(end.last_owner, &box);
}

TEST(TitleBoxTest, mapped_title_box_can_grab_the_pointer)
{
    wm::TitleBox box(100, 25);
    box.realize(root_window(), 0, 0);
    box.map();

    send(box, ButtonPress);
    send(box, ButtonRelease); // releases the grab
}

TEST(TitleBoxTest, destructor_disconnects_handlers)
{
    ::Window window;
    {
        wm::TitleBox box(100, 25);
        box.realize(root_window(), 0, 0);
        window = box.get_window();
        EXPECT_EQ(tiny::handlers.count({ButtonPress, window}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({ButtonPress, window}), 0u);
    EXPECT_EQ(tiny::handlers.count({ButtonRelease, window}), 0u);
    EXPECT_EQ(tiny::handlers.count({MotionNotify, window}), 0u);
}

TEST(TitleBoxTest, destroying_unrealized_title_box_causes_no_x_error)
{
    const XErrors x;
    {
        const wm::TitleBox box(100, 25);
    }

    EXPECT_EQ(x.errors(), 0);
}

TEST(TitleBoxTest, delete_through_base_pointer)
{
    const tiny::Widget* box = new wm::TitleBox(100, 25);

    delete box;
}

// ---- Header ----

class HeaderTest : public ::testing::Test {
  protected:
    uint32_t bg() const { return tiny::theme.wm_header.normal_bg; }

    // Drawing needs a font, which theme.wm_header does not have by default.
    // It is opened for the test and closed while the display is still open.
    void SetUp() override
    {
        original = tiny::theme.wm_header;
        with_font = original;
        with_font.init();
        tiny::theme.wm_header = with_font;
    }

    void TearDown() override { tiny::theme.wm_header = original; }

    Image image_of(const wm::Header& h)
    {
        return Image(h.get_window(), h.get_width(), h.get_height());
    }

    // Darkest red channel of the text area (without the bottom line).
    unsigned int darkest_text(const Image& img)
    {
        unsigned int darkest = 255;
        for (int y = 0; y < img.height - 1; ++y) {
            for (int x = 0; x < img.width; ++x) {
                darkest = std::min(darkest, (img.at(x, y) >> 16) & 0xFFu);
            }
        }
        return darkest;
    }

    tiny::WMHeaderStyle original;
    tiny::WMHeaderStyle with_font;
};

TEST_F(HeaderTest, new_header_is_horizontal_box_without_title)
{
    const wm::Header h(300, 25);

    EXPECT_EQ(h.get_type(), tiny::Box::Type::Horizontal);
    EXPECT_EQ(h.get_title(), "");
    EXPECT_EQ(h.get_width(), 300u);
    EXPECT_EQ(h.get_height(), 25u);
    EXPECT_EQ(&h.get_style(), &tiny::theme.wm_header);
}

TEST_F(HeaderTest, title_box_covers_the_header)
{
    wm::Header h(300, 25);

    EXPECT_EQ(h.get_title_box()->get_width(), 300u);
    EXPECT_EQ(h.get_title_box()->get_height(), 25u);
}

TEST_F(HeaderTest, realize_adds_title_box_inside_header)
{
    wm::Header h(300, 25);

    h.realize(root_window(), 0, 0);

    const wm::TitleBox* box = h.get_title_box();
    EXPECT_TRUE(box->get_realized());
    EXPECT_EQ(box->get_parent_window(), h.get_window());
    const XWindowAttributes attrs = attributes_of(*box);
    EXPECT_EQ(attrs.x, 0);
    EXPECT_EQ(attrs.y, 0);
}

TEST_F(HeaderTest, set_title_keeps_the_title)
{
    wm::Header h(300, 25);

    h.set_title("Window title");

    EXPECT_EQ(h.get_title(), "Window title");
}

TEST_F(HeaderTest, expose_draws_title_text_and_bottom_line)
{
    wm::Header h(300, 25);
    h.realize(root_window(), 0, 0);
    h.map_all();
    h.set_title("Window title");

    send(h, Expose);

    const Image img = image_of(h);
    EXPECT_EQ(img.at(0, 0), bg());
    EXPECT_GT(img.count_not(bg(), 24), 0u);                    // text
    EXPECT_EQ(img.at(5, 24), tiny::theme.wm_header.normal_br); // line
}

TEST_F(HeaderTest, header_without_title_draws_only_the_bottom_line)
{
    wm::Header h(300, 25);
    h.realize(root_window(), 0, 0);
    h.map_all();

    send(h, Expose);

    const Image img = image_of(h);
    EXPECT_EQ(img.count_not(bg(), 24), 0u);
    EXPECT_EQ(img.at(5, 24), tiny::theme.wm_header.normal_br);
}

TEST_F(HeaderTest, set_title_redraws_mapped_header)
{
    wm::Header h(300, 25);
    h.realize(root_window(), 0, 0);
    h.map_all();
    send(h, Expose);
    ASSERT_EQ(image_of(h).count_not(bg(), 24), 0u);

    h.set_title("Window title");

    EXPECT_GT(image_of(h).count_not(bg(), 24), 0u);
}

TEST_F(HeaderTest, long_title_starts_at_the_left_padding)
{
    wm::Header h(60, 25);
    h.realize(root_window(), 0, 0);
    h.map_all();

    h.set_title("A very long title that does not fit into the header");

    const Image img = image_of(h);
    EXPECT_GT(img.count_not(bg(), 24), 0u);
    for (int y = 0; y < 24; ++y) {
        EXPECT_EQ(img.at(0, y), bg()) << "row " << y; // not cut on the left
    }
}

TEST_F(HeaderTest, set_disable_makes_the_text_lighter)
{
    wm::Header h(300, 25);
    h.realize(root_window(), 0, 0);
    h.map_all();
    h.set_title("Window title");
    const unsigned int active = darkest_text(image_of(h));

    h.set_disable(true);
    const unsigned int disabled = darkest_text(image_of(h));

    EXPECT_LT(active, 0x40u);
    EXPECT_GT(disabled, 0x60u);

    h.set_disable(false);
    EXPECT_EQ(darkest_text(image_of(h)), active);
}

TEST_F(HeaderTest, set_disable_before_map_does_not_draw)
{
    wm::Header h(300, 25);
    h.realize(root_window(), 0, 0);

    h.set_disable(true); // not mapped: no repaint, no crash
}

TEST_F(HeaderTest, resize_before_realize_changes_header_and_title_box)
{
    wm::Header h(300, 25);

    h.resize(400, 30);

    EXPECT_EQ(h.get_width(), 400u);
    EXPECT_EQ(h.get_height(), 30u);
    EXPECT_EQ(h.get_title_box()->get_width(), 400u);
    EXPECT_EQ(h.get_title_box()->get_height(), 30u);
}

TEST_F(HeaderTest, resize_realized_header_resizes_both_windows)
{
    wm::Header h(300, 25);
    h.realize(root_window(), 0, 0);

    h.resize(400, 30);

    EXPECT_EQ(attributes_of(h).width, 400);
    EXPECT_EQ(attributes_of(h).height, 30);
    EXPECT_EQ(attributes_of(*h.get_title_box()).width, 400);
    EXPECT_EQ(attributes_of(*h.get_title_box()).height, 30);
}

TEST_F(HeaderTest, resize_mapped_header_redraws)
{
    wm::Header h(300, 25);
    h.realize(root_window(), 0, 0);
    h.map_all();
    h.set_title("Window title");

    h.resize(400, 30);
    // the size is applied when the server reports it, then the header is
    // exposed again
    XEvent configure = make_event(ConfigureNotify);
    configure.xconfigure.window = h.get_window();
    configure.xconfigure.width = 400;
    configure.xconfigure.height = 30;
    send(h.get_window(), configure);
    send(h, Expose);

    const Image img = image_of(h);
    EXPECT_EQ(img.width, 400u);
    EXPECT_EQ(img.at(5, 29), tiny::theme.wm_header.normal_br);
}

TEST_F(HeaderTest, title_box_signals_are_reachable_through_the_header)
{
    wm::Header h(300, 25);
    Recorder begin;
    h.get_title_box()->on_drag_begin.connect(&begin, begin.slot());
    h.realize(root_window(), 0, 0);

    send(*h.get_title_box(), ButtonPress);
    send(*h.get_title_box(), ButtonRelease);

    EXPECT_EQ(begin.calls, 1);
    EXPECT_EQ(begin.last_owner, h.get_title_box());
}

TEST_F(HeaderTest, destructor_disconnects_expose_handler)
{
    ::Window window;
    {
        wm::Header h(300, 25);
        h.realize(root_window(), 0, 0);
        window = h.get_window();
        EXPECT_EQ(tiny::handlers.count({Expose, window}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({Expose, window}), 0u);
}

TEST_F(HeaderTest, destroying_unrealized_header_causes_no_x_error)
{
    const XErrors x;
    {
        const wm::Header h(300, 25);
    }

    EXPECT_EQ(x.errors(), 0);
}

TEST_F(HeaderTest, delete_through_base_pointer)
{
    const tiny::Widget* h = new wm::Header(300, 25);

    delete h;
}
