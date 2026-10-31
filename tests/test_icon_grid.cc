#include "helpers.h"

#include "icon_grid.h"

namespace {

constexpr int columns = 4;
constexpr int rows = 2;
constexpr int cell_width = 50;
constexpr int cell_height = 40;
constexpr int dot = 6;
constexpr int grid_width = columns * cell_width;
constexpr int grid_height = rows * cell_height + 3 * dot;

tiny::IconGrid::ItemFn numbered(const std::string& prefix = "cell ")
{
    return [prefix](size_t cell) {
        tiny::IconGrid::Item item;
        item.text = prefix + std::to_string(cell);
        return item;
    };
}

XEvent button(int type, unsigned int which, int x, int y)
{
    XEvent e = make_event(type);
    e.xbutton.button = which;
    e.xbutton.x = x;
    e.xbutton.y = y;
    return e;
}

uint32_t background(bool selected)
{
    const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
    return tiny::theme.list.get_bg(
            selected ? static_cast<uint8_t>(
                               normal | static_cast<uint8_t>(
                                                tiny::Style::State::Selected))
                     : normal);
}

uint32_t foreground_of_selected()
{
    const auto selected = static_cast<uint8_t>(
            static_cast<uint8_t>(tiny::Style::State::Normal) |
            static_cast<uint8_t>(tiny::Style::State::Selected));
    return tiny::theme.list.get_fg(selected);
}

} // namespace

// 20 cells are 3 pages of 8 cells
class IconGridTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        grid.on_activate.connect(&activated, activated.slot());
        grid.realize(root_window(), 0, 0);
        grid.map();
        grid.set_items(20, numbered());
    }

    void click(int x, int y)
    {
        send(grid.get_window(), button(ButtonPress, Button1, x, y));
        send(grid.get_window(), button(ButtonRelease, Button1, x, y));
    }

    // the middle of the cell on the current page
    void click_cell(int column, int row)
    {
        click(column * cell_width + 5, row * cell_height + 5);
    }

    uint32_t pixel(int x, int y)
    {
        return Image(grid.get_window(), grid_width, grid_height).at(x, y);
    }

    WidgetFont font;
    tiny::IconGrid grid{columns, rows, cell_width, cell_height, 16, dot};
    Recorder activated;
};

TEST_F(IconGridTest, uses_the_list_style_without_a_border)
{
    EXPECT_EQ(&grid.get_style(), &tiny::theme.list);
    EXPECT_EQ(attributes_of(grid).border_width, 0);
}

TEST_F(IconGridTest, the_size_holds_the_cells_and_the_dots)
{
    EXPECT_EQ(grid.get_width(), grid_width);
    EXPECT_EQ(grid.get_height(), grid_height);
}

TEST_F(IconGridTest, cells_make_pages)
{
    EXPECT_EQ(grid.get_count(), 20u);
    EXPECT_EQ(grid.per_page(), 8u);
    EXPECT_EQ(grid.page_count(), 3u);
    EXPECT_EQ(grid.get_page(), 0u);
}

TEST_F(IconGridTest, there_is_always_at_least_one_page)
{
    grid.set_items(0, numbered());
    EXPECT_EQ(grid.page_count(), 1u);

    grid.set_items(8, numbered());
    EXPECT_EQ(grid.page_count(), 1u);

    grid.set_items(9, numbered());
    EXPECT_EQ(grid.page_count(), 2u);
}

TEST_F(IconGridTest, a_nonsense_size_is_clamped)
{
    const tiny::IconGrid tiny_grid{0, -1, 0, 0, 0, 0};

    EXPECT_EQ(tiny_grid.per_page(), 1u);
    EXPECT_GT(tiny_grid.get_width(), 0);
}

TEST_F(IconGridTest, the_first_cell_is_selected_after_set_items)
{
    grid.select(11);

    grid.set_items(3, numbered());

    EXPECT_EQ(grid.get_selected(), 0u);
    EXPECT_EQ(grid.get_page(), 0u);
}

TEST_F(IconGridTest, select_clamps_to_the_last_cell)
{
    grid.select(100);

    EXPECT_EQ(grid.get_selected(), 19u);
    EXPECT_EQ(grid.get_page(), 2u);
}

TEST_F(IconGridTest, select_in_an_empty_grid_does_nothing)
{
    grid.set_items(0, numbered());

    grid.select(3);
    grid.select_next();
    grid.select_down();

    EXPECT_EQ(grid.get_selected(), 0u);
}

TEST_F(IconGridTest, next_and_prev_go_through_the_pages)
{
    for (int i = 0; i < 8; ++i) {
        grid.select_next();
    }
    EXPECT_EQ(grid.get_selected(), 8u);
    EXPECT_EQ(grid.get_page(), 1u);

    grid.select_prev();
    EXPECT_EQ(grid.get_selected(), 7u);
    EXPECT_EQ(grid.get_page(), 0u);
}

TEST_F(IconGridTest, prev_stays_at_the_first_cell_and_next_at_the_last)
{
    grid.select_prev();
    EXPECT_EQ(grid.get_selected(), 0u);

    grid.select(19);
    grid.select_next();
    EXPECT_EQ(grid.get_selected(), 19u);
}

TEST_F(IconGridTest, down_and_up_move_by_a_row)
{
    grid.select(2);

    grid.select_down();
    EXPECT_EQ(grid.get_selected(), 6u);

    grid.select_up();
    EXPECT_EQ(grid.get_selected(), 2u);
}

TEST_F(IconGridTest, down_from_the_last_row_goes_to_the_next_page)
{
    grid.select(5);

    grid.select_down();

    EXPECT_EQ(grid.get_selected(), 9u);
    EXPECT_EQ(grid.get_page(), 1u);

    grid.select_up();
    EXPECT_EQ(grid.get_selected(), 5u);
    EXPECT_EQ(grid.get_page(), 0u);
}

TEST_F(IconGridTest, up_from_the_first_row_stays)
{
    grid.select(2);

    grid.select_up();

    EXPECT_EQ(grid.get_selected(), 2u);
}

TEST_F(IconGridTest, down_to_a_shorter_row_selects_the_last_cell)
{
    grid.select(14); // the row 16..19 is the last one, 14 is in the row 12..15

    grid.select_down();
    EXPECT_EQ(grid.get_selected(), 18u);

    grid.select(15);
    grid.select_down();
    EXPECT_EQ(grid.get_selected(), 19u);

    grid.select_down(); // no row below
    EXPECT_EQ(grid.get_selected(), 19u);
}

TEST_F(IconGridTest, down_to_a_missing_cell_selects_the_last_one)
{
    grid.set_items(6, numbered()); // 4 + 2 cells
    grid.select(3);

    grid.select_down();

    EXPECT_EQ(grid.get_selected(), 5u);
}

TEST_F(IconGridTest, pages_keep_the_place_in_the_page)
{
    grid.select(3);

    grid.select_page(true);
    EXPECT_EQ(grid.get_selected(), 11u);

    grid.select_page(true);
    EXPECT_EQ(grid.get_selected(), 19u); // the last page has only 4 cells

    grid.select_page(true); // nothing after the last page
    EXPECT_EQ(grid.get_selected(), 19u);

    grid.select_page(false);
    EXPECT_EQ(grid.get_selected(), 11u);

    grid.select_page(false);
    grid.select_page(false); // nothing before the first page
    EXPECT_EQ(grid.get_selected(), 3u);
}

TEST_F(IconGridTest, cell_at_finds_the_cell_of_the_current_page)
{
    EXPECT_EQ(grid.cell_at(0, 0), 0u);
    EXPECT_EQ(grid.cell_at(cell_width * 3 + 1, 1), 3u);
    EXPECT_EQ(grid.cell_at(cell_width + 1, cell_height + 1), 5u);

    grid.select(8);
    EXPECT_EQ(grid.cell_at(0, 0), 8u);
}

TEST_F(IconGridTest, cell_at_is_none_outside_of_the_cells)
{
    EXPECT_EQ(grid.cell_at(-1, 5), 20u);
    EXPECT_EQ(grid.cell_at(5, -1), 20u);
    EXPECT_EQ(grid.cell_at(grid_width, 5), 20u);
    EXPECT_EQ(grid.cell_at(5, rows * cell_height), 20u); // the dots
}

TEST_F(IconGridTest, cell_at_is_none_after_the_last_cell)
{
    grid.select(19); // the third page: 16..19

    EXPECT_EQ(grid.cell_at(cell_width * 3 + 1, 1), 19u);
    EXPECT_EQ(grid.cell_at(1, cell_height + 1), 20u);
}

TEST_F(IconGridTest, dot_at_finds_the_dot)
{
    // 3 dots, 6 px each with the 6 px gaps, centered: 85 .. 115
    EXPECT_EQ(grid.dot_at(88, rows * cell_height + dot + 2), 0u);
    EXPECT_EQ(grid.dot_at(100, rows * cell_height + dot + 2), 1u);
    EXPECT_EQ(grid.dot_at(112, rows * cell_height + dot + 2), 2u);
}

TEST_F(IconGridTest, dot_at_is_none_outside_of_the_dots)
{
    const int y = rows * cell_height + dot + 2;

    EXPECT_EQ(grid.dot_at(10, y), 3u);
    EXPECT_EQ(grid.dot_at(190, y), 3u);
    EXPECT_EQ(grid.dot_at(88, 5), 3u);
    EXPECT_EQ(grid.dot_at(88, grid_height), 3u);
}

TEST_F(IconGridTest, there_are_no_dots_for_one_page)
{
    grid.set_items(8, numbered());

    EXPECT_EQ(grid.dot_at(100, rows * cell_height + dot + 2), 1u);
}

TEST_F(IconGridTest, click_on_a_cell_selects_and_activates_it)
{
    click_cell(2, 1);

    EXPECT_EQ(grid.get_selected(), 6u);
    EXPECT_EQ(activated.calls, 1);
}

TEST_F(IconGridTest, click_on_a_cell_of_another_page_uses_the_current_page)
{
    grid.select(8);

    click_cell(1, 0);

    EXPECT_EQ(grid.get_selected(), 9u);
}

TEST_F(IconGridTest, click_outside_of_the_cells_does_nothing)
{
    grid.set_items(2, numbered());

    click_cell(3, 1);

    EXPECT_EQ(grid.get_selected(), 0u);
    EXPECT_EQ(activated.calls, 0);
}

TEST_F(IconGridTest, release_on_another_cell_does_not_activate)
{
    send(grid.get_window(), button(ButtonPress, Button1, 2, 2));
    send(grid.get_window(), button(ButtonRelease, Button1, cell_width + 2, 2));

    EXPECT_EQ(activated.calls, 0);
}

TEST_F(IconGridTest, other_buttons_do_not_activate)
{
    send(grid.get_window(), button(ButtonPress, Button3, 2, 2));
    send(grid.get_window(), button(ButtonRelease, Button3, 2, 2));

    EXPECT_EQ(activated.calls, 0);
}

TEST_F(IconGridTest, click_on_a_dot_goes_to_its_page)
{
    click(112, rows * cell_height + dot + 2);

    EXPECT_EQ(grid.get_page(), 2u);
    EXPECT_EQ(grid.get_selected(), 16u);
    EXPECT_EQ(activated.calls, 0);
}

TEST_F(IconGridTest, pointer_motion_selects_the_cell_under_it)
{
    XEvent e = make_event(MotionNotify);
    e.xmotion.x = cell_width * 2 + 1;
    e.xmotion.y = cell_height + 1;

    send(grid.get_window(), e);

    EXPECT_EQ(grid.get_selected(), 6u);
    EXPECT_EQ(activated.calls, 0);
}

TEST_F(IconGridTest, motion_over_nothing_keeps_the_selection)
{
    grid.set_items(2, numbered());
    grid.select(1);
    XEvent e = make_event(MotionNotify);
    e.xmotion.x = cell_width * 3;
    e.xmotion.y = 1;

    send(grid.get_window(), e);

    EXPECT_EQ(grid.get_selected(), 1u);
}

TEST_F(IconGridTest, wheel_turns_the_pages)
{
    send(grid.get_window(), button(ButtonPress, Button5, 5, 5));
    EXPECT_EQ(grid.get_page(), 1u);

    send(grid.get_window(), button(ButtonPress, Button5, 5, 5));
    send(grid.get_window(), button(ButtonPress, Button5, 5, 5));
    EXPECT_EQ(grid.get_page(), 2u);

    send(grid.get_window(), button(ButtonPress, Button4, 5, 5));
    EXPECT_EQ(grid.get_page(), 1u);

    send(grid.get_window(), button(ButtonPress, Button4, 5, 5));
    send(grid.get_window(), button(ButtonPress, Button4, 5, 5));
    EXPECT_EQ(grid.get_page(), 0u);
}

TEST_F(IconGridTest, other_wheel_buttons_are_ignored)
{
    send(grid.get_window(), button(ButtonPress, Button2, 5, 5));

    EXPECT_EQ(grid.get_page(), 0u);
}

TEST_F(IconGridTest, the_selected_cell_is_lighter)
{
    EXPECT_EQ(pixel(3, 3), background(true));
    EXPECT_EQ(pixel(cell_width + 3, 3), background(false));
}

TEST_F(IconGridTest, the_dot_of_the_current_page_is_bright)
{
    const int y = rows * cell_height + dot + dot / 2;

    EXPECT_EQ(pixel(88, y), foreground_of_selected());
    EXPECT_EQ(pixel(100, y), background(true));
    EXPECT_EQ(pixel(112, y), background(true));
}

TEST_F(IconGridTest, one_page_has_no_dots)
{
    grid.set_items(8, numbered());

    const Image image(grid.get_window(), grid_width, grid_height);

    for (int x = 0; x < grid_width; ++x) {
        EXPECT_EQ(image.at(x, rows * cell_height + dot + dot / 2),
                  background(false));
    }
}

TEST_F(IconGridTest, a_long_text_stays_in_its_cell)
{
    grid.set_items(1, numbered(std::string(60, 'W')));

    const Image image(grid.get_window(), grid_width, grid_height);

    unsigned int outside = 0;
    for (int y = 0; y < rows * cell_height; ++y) {
        for (int x = cell_width; x < grid_width; ++x) {
            outside += image.at(x, y) != background(false);
        }
    }
    EXPECT_EQ(outside, 0u);
}

TEST_F(IconGridTest, a_text_with_multibyte_characters_is_cut_safely)
{
    grid.set_items(1,
                   numbered(std::string(30, 'W') + "\xC4\x9B\xC5\xA1\xC4\x8D"));

    send(grid, Expose);

    EXPECT_EQ(grid.get_count(), 1u);
}

TEST_F(IconGridTest, drawing_without_items_or_server_pictures_is_safe)
{
    tiny::IconGrid empty{2, 1, 30, 30, 16, 4};
    empty.realize(root_window(), 0, 0);
    empty.map();

    send(empty, Expose);

    EXPECT_EQ(empty.get_count(), 0u);
}

TEST_F(IconGridTest, many_pages_have_dots_closer_to_each_other)
{
    grid.set_items(8 * 30, numbered()); // 30 pages: 6 px apart, from x = 10

    const int y = rows * cell_height + dot + 2;
    EXPECT_EQ(grid.dot_at(11, y), 0u);
    EXPECT_EQ(grid.dot_at(10 + 29 * 6 + 3, y), 29u);
    EXPECT_EQ(grid.dot_at(2, y), 30u);
}

TEST_F(IconGridTest, release_after_a_press_on_a_dot_does_not_activate)
{
    click_cell(0, 0);
    ASSERT_EQ(activated.calls, 1);
    const int y = rows * cell_height + dot + 2;

    send(grid.get_window(), button(ButtonPress, Button1, 100, y));
    send(grid.get_window(), button(ButtonRelease, Button1, 5, 5));

    EXPECT_EQ(activated.calls, 1);
}
