#include "helpers.h"

#include "image/pixmap.h"
#include "list_box.h"

namespace {

constexpr int list_width = 200;
constexpr int row_height = 20;
constexpr int rows = 5;

tiny::ListBox::ItemFn numbered(const std::string& prefix = "row ")
{
    return [prefix](size_t row) {
        tiny::ListBox::Item item;
        item.text = prefix + std::to_string(row);
        return item;
    };
}

XEvent button(int type, unsigned int which, int y)
{
    XEvent e = make_event(type);
    e.xbutton.button = which;
    e.xbutton.x = 10;
    e.xbutton.y = y;
    return e;
}

} // namespace

class ListBoxTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        list.on_activate.connect(&activated, activated.slot());
        list.realize(root_window(), 0, 0);
        list.map();
        list.set_items(12, numbered());
    }

    void click(int y)
    {
        send(list.get_window(), button(ButtonPress, Button1, y));
        send(list.get_window(), button(ButtonRelease, Button1, y));
    }

    uint32_t pixel(int x, int y)
    {
        return Image(list.get_window(), list_width, rows * row_height).at(x, y);
    }

    static uint32_t background(bool selected)
    {
        const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
        return tiny::theme.list.get_bg(
                selected ? static_cast<uint8_t>(
                                   normal |
                                   static_cast<uint8_t>(
                                           tiny::Style::State::Selected))
                         : normal);
    }

    WidgetFont font;
    tiny::ListBox list{list_width, rows* row_height, row_height, 16};
    Recorder activated;
};

TEST_F(ListBoxTest, uses_the_list_style_without_a_border)
{
    EXPECT_EQ(&list.get_style(), &tiny::theme.list);
    EXPECT_EQ(attributes_of(list).border_width, 0);
}

TEST_F(ListBoxTest, shows_as_many_rows_as_fit)
{
    EXPECT_EQ(list.visible_rows(), 5u);
    EXPECT_EQ(list.get_count(), 12u);
}

TEST_F(ListBoxTest, there_is_always_at_least_one_visible_row)
{
    const tiny::ListBox small{50, 5, row_height, 16};

    EXPECT_EQ(small.visible_rows(), 1u);
}

TEST_F(ListBoxTest, the_first_row_is_selected_after_set_items)
{
    list.select(7);

    list.set_items(3, numbered());

    EXPECT_EQ(list.get_selected(), 0u);
    EXPECT_EQ(list.get_first(), 0u);
}

TEST_F(ListBoxTest, select_clamps_to_the_last_row)
{
    list.select(100);

    EXPECT_EQ(list.get_selected(), 11u);
    EXPECT_EQ(list.get_first(), 7u);
}

TEST_F(ListBoxTest, next_and_prev_scroll_when_the_selection_leaves_the_view)
{
    for (int i = 0; i < 5; ++i) {
        list.select_next();
    }
    EXPECT_EQ(list.get_selected(), 5u);
    EXPECT_EQ(list.get_first(), 1u);

    for (int i = 0; i < 5; ++i) {
        list.select_prev();
    }
    EXPECT_EQ(list.get_selected(), 0u);
    EXPECT_EQ(list.get_first(), 0u);

    list.select_prev(); // stays at the top
    EXPECT_EQ(list.get_selected(), 0u);
}

TEST_F(ListBoxTest, pages_move_by_the_visible_rows)
{
    list.select_page(true);
    EXPECT_EQ(list.get_selected(), 5u);

    list.select_page(true);
    list.select_page(true);
    EXPECT_EQ(list.get_selected(), 11u);

    list.select_page(false);
    EXPECT_EQ(list.get_selected(), 6u);

    list.select_page(false);
    list.select_page(false);
    EXPECT_EQ(list.get_selected(), 0u);
}

TEST_F(ListBoxTest, an_empty_list_keeps_the_selection_at_zero)
{
    list.set_items(0, numbered());

    list.select(3);
    list.select_next();
    list.select_page(true);

    EXPECT_EQ(list.get_selected(), 0u);
    EXPECT_EQ(list.row_at(5), 0u); // the count: there is no row
}

TEST_F(ListBoxTest, row_at_follows_the_scrolling)
{
    EXPECT_EQ(list.row_at(0), 0u);
    EXPECT_EQ(list.row_at(row_height * 2 + 3), 2u);
    EXPECT_EQ(list.row_at(-1), 12u);

    list.select(11);

    EXPECT_EQ(list.row_at(0), 7u);
}

TEST_F(ListBoxTest, row_at_below_the_last_row_is_the_count)
{
    list.set_items(2, numbered());

    EXPECT_EQ(list.row_at(row_height), 1u);
    EXPECT_EQ(list.row_at(row_height * 2), 2u);
}

TEST_F(ListBoxTest, selected_row_has_the_selected_background)
{
    list.select(1);

    EXPECT_EQ(pixel(list_width - 2, 5), background(false));
    EXPECT_EQ(pixel(list_width - 2, row_height + 5), background(true));
}

TEST_F(ListBoxTest, rows_draw_their_text)
{
    send(list, Expose);

    const Image image(list.get_window(), list_width, rows * row_height);
    EXPECT_GT(image.count_not(background(true), row_height), 0u);
    EXPECT_GT(image.count_not(background(false), 2 * row_height), 0u);
}

TEST_F(ListBoxTest, icon_is_drawn_in_front_of_the_text)
{
    tiny::image::Image red;
    red.width = 16;
    red.height = 16;
    red.pixels.assign(16 * 16, 0xFFFF0000u);
    const Pixmap pixmap =
            tiny::image::create_pixmap(tiny::get_display(), root_window(), red);
    ASSERT_NE(pixmap, static_cast<Pixmap>(None));
    const Picture picture =
            tiny::image::create_argb_picture(tiny::get_display(), pixmap);
    ASSERT_NE(picture, static_cast<Picture>(None));

    list.set_items(1, [picture](size_t) {
        tiny::ListBox::Item item;
        item.text = "with an icon";
        item.icon = picture;
        item.icon_width = 16;
        item.icon_height = 16;
        return item;
    });

    const int padding = tiny::theme.list.padding_width;
    EXPECT_EQ(pixel(padding + 8, (row_height - 16) / 2 + 8), 0xFF0000u);

    XRenderFreePicture(tiny::get_display(), picture);
    XFreePixmap(tiny::get_display(), pixmap);
}

TEST_F(ListBoxTest, click_selects_and_activates_the_row)
{
    click(row_height * 3 + 2);

    EXPECT_EQ(list.get_selected(), 3u);
    EXPECT_EQ(activated.calls, 1);
    EXPECT_EQ(activated.last_owner, &list);
}

TEST_F(ListBoxTest, click_below_the_rows_does_nothing)
{
    list.set_items(2, numbered());

    click(row_height * 3);

    EXPECT_EQ(list.get_selected(), 0u);
    EXPECT_EQ(activated.calls, 0);
}

TEST_F(ListBoxTest, release_on_another_row_does_not_activate)
{
    send(list.get_window(), button(ButtonPress, Button1, 2));
    send(list.get_window(), button(ButtonRelease, Button1, row_height + 2));

    EXPECT_EQ(activated.calls, 0);
}

TEST_F(ListBoxTest, other_buttons_do_not_activate)
{
    send(list.get_window(), button(ButtonPress, Button3, 2));
    send(list.get_window(), button(ButtonRelease, Button3, 2));

    EXPECT_EQ(activated.calls, 0);
}

TEST_F(ListBoxTest, pointer_motion_selects_the_row_under_it)
{
    XEvent e = make_event(MotionNotify);
    e.xmotion.y = row_height * 4 + 1;

    send(list.get_window(), e);

    EXPECT_EQ(list.get_selected(), 4u);
    EXPECT_EQ(activated.calls, 0);
}

TEST_F(ListBoxTest, motion_below_the_rows_keeps_the_selection)
{
    list.set_items(2, numbered());
    list.select(1);
    XEvent e = make_event(MotionNotify);
    e.xmotion.y = row_height * 4;

    send(list.get_window(), e);

    EXPECT_EQ(list.get_selected(), 1u);
}

TEST_F(ListBoxTest, wheel_scrolls_the_rows)
{
    send(list.get_window(), button(ButtonPress, Button5, 5));
    EXPECT_EQ(list.get_first(), 3u);

    send(list.get_window(), button(ButtonPress, Button5, 5));
    send(list.get_window(), button(ButtonPress, Button5, 5));
    send(list.get_window(), button(ButtonPress, Button5, 5));
    EXPECT_EQ(list.get_first(), 7u); // the last page

    send(list.get_window(), button(ButtonPress, Button4, 5));
    EXPECT_EQ(list.get_first(), 4u);

    send(list.get_window(), button(ButtonPress, Button4, 5));
    send(list.get_window(), button(ButtonPress, Button4, 5));
    EXPECT_EQ(list.get_first(), 0u);
}

TEST_F(ListBoxTest, wheel_keeps_the_selected_row_in_view)
{
    send(list.get_window(), button(ButtonPress, Button5, 5));
    EXPECT_EQ(list.get_selected(), 3u); // row 0 left the view

    send(list.get_window(), button(ButtonPress, Button4, 5));
    send(list.get_window(), button(ButtonPress, Button4, 5));
    EXPECT_EQ(list.get_first(), 0u);
    EXPECT_EQ(list.get_selected(), 3u); // still visible, so it stays

    list.select(4);
    send(list.get_window(), button(ButtonPress, Button5, 5));
    send(list.get_window(), button(ButtonPress, Button5, 5));
    send(list.get_window(), button(ButtonPress, Button5, 5));
    EXPECT_EQ(list.get_first(), 7u);
    EXPECT_EQ(list.get_selected(), 7u);

    send(list.get_window(), button(ButtonPress, Button4, 5));
    send(list.get_window(), button(ButtonPress, Button4, 5));
    EXPECT_EQ(list.get_first(), 1u);
    EXPECT_EQ(list.get_selected(), 5u); // the last visible row
}

TEST_F(ListBoxTest, other_wheel_buttons_are_ignored)
{
    send(list.get_window(), button(ButtonPress, Button2, 5));

    EXPECT_EQ(list.get_first(), 0u);
}

TEST_F(ListBoxTest, drawing_without_items_or_server_pictures_is_safe)
{
    tiny::ListBox empty{50, 40, row_height, 16};
    empty.realize(root_window(), 0, 0);
    empty.map();

    send(empty, Expose);

    EXPECT_EQ(empty.get_count(), 0u);
}
