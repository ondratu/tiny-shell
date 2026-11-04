#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "font_browser.h"

namespace {

using tiny::FontBrowser;
using Key = FontBrowser::Key;
using View = FontBrowser::View;

//! Small round numbers: three columns of 100 x 50 in a window of 320 x 120
FontBrowser::Metrics metrics()
{
    FontBrowser::Metrics m;
    m.cell_width = 100;
    m.cell_height = 50;
    m.margin = 10;
    m.header = 20;
    m.back_width = 40;
    return m;
}

std::vector<std::string> names(size_t count)
{
    std::vector<std::string> list;
    list.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        list.push_back("Font " + std::to_string(i));
    }
    return list;
}

//! Ten families in four rows (the last one has one cell)
FontBrowser browser(size_t count = 10)
{
    FontBrowser b(names(count), metrics());
    b.resize(320, 120);
    return b;
}

} // namespace

TEST(FontBrowserTest, the_columns_fit_the_width_but_there_is_always_one)
{
    FontBrowser b = browser();
    EXPECT_EQ(b.columns(), 3u);

    b.resize(319, 120);
    EXPECT_EQ(b.columns(), 2u);

    b.resize(30, 120);
    EXPECT_EQ(b.columns(), 1u);
}

TEST(FontBrowserTest, the_content_is_the_rows_and_the_margins)
{
    const FontBrowser b = browser();

    EXPECT_EQ(b.content_height(), 2 * 10 + 4 * 50);
}

TEST(FontBrowserTest, the_cells_are_centered_in_a_wider_window)
{
    FontBrowser b = browser();
    b.resize(400, 120);

    const tiny::Rect first = b.cell_rect(0);
    EXPECT_EQ(first.x, 50);
    EXPECT_EQ(first.y, 10);
    EXPECT_EQ(first.width, 100);
    EXPECT_EQ(first.height, 50);
    EXPECT_EQ(b.cell_rect(4).x, 150);
    EXPECT_EQ(b.cell_rect(4).y, 60);
}

TEST(FontBrowserTest, the_cells_of_a_window_narrower_than_a_cell_start_at_zero)
{
    FontBrowser b = browser();
    b.resize(60, 120);

    EXPECT_EQ(b.cell_rect(0).x, 0);
}

TEST(FontBrowserTest, the_visible_cells_are_the_rows_that_show)
{
    FontBrowser b = browser();

    EXPECT_EQ(b.visible_cells(), (std::pair<size_t, size_t>{0, 9}));

    b.wheel(100);
    EXPECT_EQ(b.scroll(), 100);
    // only the last two rows show: 10 + 3 * 50 - 100 is under the top
    EXPECT_EQ(b.visible_cells(), (std::pair<size_t, size_t>{3, 10}));
}

TEST(FontBrowserTest, no_cells_are_visible_without_families)
{
    FontBrowser b({}, metrics());
    b.resize(320, 120);

    EXPECT_EQ(b.visible_cells(), (std::pair<size_t, size_t>{0, 0}));
    EXPECT_TRUE(b.family().empty());
    EXPECT_EQ(b.cell_at(50, 30), 0u);
}

TEST(FontBrowserTest, a_point_finds_its_cell_or_the_count)
{
    const FontBrowser b = browser();

    EXPECT_EQ(b.cell_at(10, 10), 0u);
    EXPECT_EQ(b.cell_at(110, 10), 1u);
    EXPECT_EQ(b.cell_at(210, 60), 5u);
    // the margin, the gap at the side, the empty cells of the last row
    EXPECT_EQ(b.cell_at(5, 30), 10u);
    EXPECT_EQ(b.cell_at(50, 5), 10u);
    EXPECT_EQ(b.cell_at(315, 30), 10u);
    EXPECT_EQ(b.cell_at(210, 160), 10u); // the second cell of the last row
}

TEST(FontBrowserTest, a_point_follows_the_scrolling)
{
    FontBrowser b = browser();
    b.wheel(3);

    EXPECT_EQ(b.scroll(), 48);
    // the second row is now at 10 + 50 - 48 = 12
    EXPECT_EQ(b.cell_at(10, 15), 3u);
}

TEST(FontBrowserTest, the_wheel_stays_inside_of_the_content)
{
    FontBrowser b = browser();

    EXPECT_FALSE(b.wheel(-1));
    EXPECT_TRUE(b.wheel(1));
    EXPECT_EQ(b.scroll(), 16);
    EXPECT_TRUE(b.wheel(100));
    EXPECT_EQ(b.scroll(), 100); // 220 - 120
    EXPECT_FALSE(b.wheel(1));
    EXPECT_TRUE(b.wheel(-100));
    EXPECT_EQ(b.scroll(), 0);
}

TEST(FontBrowserTest, a_bigger_window_scrolls_back_into_the_content)
{
    FontBrowser b = browser();
    b.wheel(100);

    b.resize(320, 400);

    EXPECT_EQ(b.scroll(), 0);
}

TEST(FontBrowserTest, the_keys_move_the_selection_by_cells_and_rows)
{
    FontBrowser b = browser();

    EXPECT_FALSE(b.key(Key::Left)); // nothing before the first one
    EXPECT_FALSE(b.key(Key::Up));
    EXPECT_TRUE(b.key(Key::Right));
    EXPECT_EQ(b.selected(), 1u);
    EXPECT_TRUE(b.key(Key::Down));
    EXPECT_EQ(b.selected(), 4u);
    EXPECT_TRUE(b.key(Key::Up));
    EXPECT_EQ(b.selected(), 1u);
    EXPECT_TRUE(b.key(Key::Left));
    EXPECT_EQ(b.selected(), 0u);
}

TEST(FontBrowserTest, the_selection_stops_at_the_last_cell)
{
    FontBrowser b = browser();

    EXPECT_TRUE(b.key(Key::End));
    EXPECT_EQ(b.selected(), 9u);
    EXPECT_FALSE(b.key(Key::End));
    EXPECT_FALSE(b.key(Key::Right));
    EXPECT_FALSE(b.key(Key::PageDown));
    EXPECT_EQ(b.selected(), 9u);
}

TEST(FontBrowserTest, down_stays_when_there_is_no_cell_under_the_selection)
{
    FontBrowser b = browser();
    b.key(Key::End);
    b.key(Key::Left); // 8, in the third row

    EXPECT_EQ(b.selected(), 8u);
    EXPECT_FALSE(b.key(Key::Down)); // 11 would be after the last one
    EXPECT_EQ(b.selected(), 8u);
}

TEST(FontBrowserTest, the_pages_move_by_the_visible_rows)
{
    FontBrowser b = browser();

    // 120 pixels show two rows of three cells
    EXPECT_TRUE(b.key(Key::PageDown));
    EXPECT_EQ(b.selected(), 6u);
    EXPECT_TRUE(b.key(Key::PageUp));
    EXPECT_EQ(b.selected(), 0u);
    EXPECT_FALSE(b.key(Key::PageUp));
    EXPECT_TRUE(b.key(Key::PageDown));
    EXPECT_TRUE(b.key(Key::Home));
    EXPECT_EQ(b.selected(), 0u);
}

TEST(FontBrowserTest, the_selected_cell_is_scrolled_into_view)
{
    FontBrowser b = browser();

    b.key(Key::End); // the last row
    const tiny::Rect last = b.cell_rect(9);
    EXPECT_GE(last.y, 0);
    EXPECT_LE(last.y + last.height + 10, 120);

    b.key(Key::Home);
    EXPECT_EQ(b.scroll(), 0);
    EXPECT_EQ(b.cell_rect(0).y, 10);
}

TEST(FontBrowserTest, return_opens_the_selected_family)
{
    FontBrowser b = browser();
    b.key(Key::Right);

    EXPECT_TRUE(b.key(Key::Return));

    EXPECT_EQ(b.view(), View::Detail);
    EXPECT_EQ(b.family(), "Font 1");
}

TEST(FontBrowserTest, the_keys_of_the_detail_do_nothing_in_the_overview)
{
    FontBrowser b = browser();

    EXPECT_FALSE(b.key(Key::Escape));
    EXPECT_FALSE(b.key(Key::Back));
    EXPECT_EQ(b.view(), View::Overview);
}

TEST(FontBrowserTest, nothing_to_select_or_open_without_families)
{
    FontBrowser b({}, metrics());
    b.resize(320, 120);

    EXPECT_FALSE(b.key(Key::Right));
    EXPECT_FALSE(b.key(Key::Return));
    EXPECT_EQ(b.view(), View::Overview);
}

TEST(FontBrowserTest, a_click_on_a_cell_opens_it)
{
    FontBrowser b = browser();

    b.press(110, 70);
    EXPECT_EQ(b.view(), View::Overview);
    EXPECT_TRUE(b.release(120, 80));

    EXPECT_EQ(b.view(), View::Detail);
    EXPECT_EQ(b.family(), "Font 4");
}

TEST(FontBrowserTest, a_press_that_ends_on_another_cell_does_nothing)
{
    FontBrowser b = browser();

    b.press(10, 10);

    EXPECT_FALSE(b.release(110, 10));
    EXPECT_EQ(b.view(), View::Overview);
}

TEST(FontBrowserTest, a_release_without_a_press_or_outside_does_nothing)
{
    FontBrowser b = browser();

    EXPECT_FALSE(b.release(10, 10));
    b.press(5, 5); // the margin
    EXPECT_FALSE(b.release(5, 5));
    EXPECT_EQ(b.view(), View::Overview);
}

TEST(FontBrowserTest, the_detail_scrolls_under_its_header)
{
    FontBrowser b = browser();
    b.open(2);
    b.set_detail_height(300);

    EXPECT_EQ(b.view(), View::Detail);
    EXPECT_EQ(b.top(), 20);
    EXPECT_EQ(b.view_height(), 100);
    EXPECT_EQ(b.content_height(), 300);
    EXPECT_EQ(b.visible_cells(), (std::pair<size_t, size_t>{0, 0}));
    EXPECT_EQ(b.cell_at(10, 30), 10u);

    EXPECT_FALSE(b.key(Key::Up));
    EXPECT_TRUE(b.key(Key::Down));
    EXPECT_EQ(b.scroll(), 16);
    EXPECT_TRUE(b.key(Key::PageDown));
    EXPECT_EQ(b.scroll(), 16 + 84);
    EXPECT_TRUE(b.key(Key::End));
    EXPECT_EQ(b.scroll(), 200);
    EXPECT_FALSE(b.key(Key::PageDown));
    EXPECT_TRUE(b.key(Key::PageUp));
    EXPECT_EQ(b.scroll(), 116);
    EXPECT_TRUE(b.key(Key::Up));
    EXPECT_TRUE(b.key(Key::Home));
    EXPECT_EQ(b.scroll(), 0);
    EXPECT_TRUE(b.wheel(2));
    EXPECT_EQ(b.scroll(), 32);
}

TEST(FontBrowserTest, the_height_of_the_detail_is_unknown_until_it_is_set)
{
    FontBrowser b = browser();

    b.open(1);

    EXPECT_EQ(b.content_height(), 0);
    EXPECT_FALSE(b.wheel(1));
    b.set_detail_height(-5);
    EXPECT_EQ(b.content_height(), 0);
}

TEST(FontBrowserTest, a_smaller_detail_scrolls_back_into_view)
{
    FontBrowser b = browser();
    b.open(1);
    b.set_detail_height(400);
    b.key(Key::End);

    b.resize(320, 380);

    EXPECT_EQ(b.scroll(), 40); // 400 - (380 - 20)
}

TEST(FontBrowserTest, the_keys_of_the_detail_that_do_not_scroll)
{
    FontBrowser b = browser();
    b.open(1);

    EXPECT_FALSE(b.key(Key::Left));
    EXPECT_FALSE(b.key(Key::Right));
    EXPECT_FALSE(b.key(Key::Return));
    EXPECT_EQ(b.view(), View::Detail);
}

TEST(FontBrowserTest, escape_and_backspace_return_to_the_overview)
{
    for (const Key key : {Key::Escape, Key::Back}) {
        FontBrowser b = browser();
        b.open(1);

        EXPECT_TRUE(b.key(key));

        EXPECT_EQ(b.view(), View::Overview);
    }
}

TEST(FontBrowserTest, the_overview_comes_back_where_it_was_left)
{
    FontBrowser b = browser();
    b.wheel(100);
    b.open(9);
    b.set_detail_height(300);
    b.key(Key::End);

    b.back();

    EXPECT_EQ(b.scroll(), 100);
    EXPECT_EQ(b.selected(), 9u);
}

TEST(FontBrowserTest, back_in_the_overview_does_nothing)
{
    FontBrowser b = browser();
    b.wheel(2);

    b.back();

    EXPECT_EQ(b.scroll(), 32);
}

TEST(FontBrowserTest, the_button_back_is_a_click_in_the_header)
{
    FontBrowser b = browser();
    b.open(1);

    EXPECT_EQ(b.back_rect().width, 40);
    EXPECT_EQ(b.back_rect().height, 20);
    b.press(10, 10);
    EXPECT_TRUE(b.release(12, 12));

    EXPECT_EQ(b.view(), View::Overview);
}

TEST(FontBrowserTest, a_click_elsewhere_in_the_detail_does_nothing)
{
    FontBrowser b = browser();
    b.open(1);

    b.press(200, 10); // the header, but not the button
    EXPECT_FALSE(b.release(200, 10));
    b.press(10, 10);
    EXPECT_FALSE(b.release(100, 10)); // moved away from the button

    EXPECT_EQ(b.view(), View::Detail);
}

TEST(FontBrowserTest, an_index_out_of_the_list_is_not_opened)
{
    FontBrowser b = browser();

    b.open(10);

    EXPECT_EQ(b.view(), View::Overview);
}

TEST(FontBrowserTest, the_metrics_are_scaled_and_rounded)
{
    const auto m = FontBrowser::Metrics::scaled(1.5);

    EXPECT_EQ(m.cell_width, 330);
    EXPECT_EQ(m.cell_height, 285);
    EXPECT_EQ(m.preview, 180);
    EXPECT_EQ(m.preview_baseline, 203); // 202.5
    EXPECT_EQ(m.header, 60);
    EXPECT_EQ(m.small_size, 12);
}

TEST(FontBrowserTest, the_specimen_has_the_letters_and_the_sizes)
{
    const auto lines = FontBrowser::specimen(FontBrowser::Metrics());

    ASSERT_EQ(lines.size(), 11u);
    EXPECT_EQ(lines[0].text, "abcdefghijklmnopqrstuvwxyz");
    EXPECT_EQ(lines[1].text, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    EXPECT_EQ(lines[2].text.substr(0, 10), "0123456789");
    EXPECT_NE(lines[2].text.find('!'), std::string::npos);
    EXPECT_NE(lines[2].text.find('['), std::string::npos);
    EXPECT_NE(lines[2].text.find(';'), std::string::npos);
    const int sizes[] = {8, 12, 16, 20, 24, 32, 64, 128};
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(lines[3 + i].size, sizes[i]);
        EXPECT_EQ(lines[3 + i].text.substr(0, 11), "Lorem ipsum");
    }
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(lines[i].size, 28);
    }
}

TEST(FontBrowserTest, the_sizes_of_the_specimen_follow_the_scale)
{
    const auto lines = FontBrowser::specimen(FontBrowser::Metrics::scaled(2));

    EXPECT_EQ(lines[0].size, 56);
    EXPECT_EQ(lines[3].size, 16);
    EXPECT_EQ(lines[10].size, 256);
}

TEST(FontBrowserTest, the_selected_cell_is_in_view_after_a_resize_in_the_detail)
{
    FontBrowser b = browser();
    b.key(Key::End);
    b.key(Key::Return);

    b.resize(100, 120); // one column: the cell is in the tenth row
    b.back();

    const tiny::Rect cell = b.cell_rect(9);
    EXPECT_GE(cell.y, 0);
    EXPECT_LE(cell.y + cell.height, 120);
}

TEST(FontBrowserTest, a_family_opened_again_has_no_height_until_it_is_set)
{
    FontBrowser b = browser();
    b.open(1);
    b.set_detail_height(300);
    b.back();

    b.open(1);

    EXPECT_EQ(b.content_height(), 0);
}
