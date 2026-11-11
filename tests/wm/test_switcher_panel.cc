#include "wm_helpers.h"

#include "image/pixmap.h"
#include "wm/shell_theme.h"
#include "wm/switcher_panel.h"

using tiny::wm::SwitcherCell;
using tiny::wm::SwitcherPanel;

namespace {

constexpr int icon = 32;
constexpr int cell = 80;

int border()
{
    return tiny::wm::shell_theme.dock_border;
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

std::vector<SwitcherCell> cells_of(size_t count)
{
    std::vector<SwitcherCell> list(count);
    for (size_t i = 0; i < count; ++i) {
        list[i].text = "Cell " + std::to_string(i);
    }
    return list;
}

} // namespace

class SwitcherPanelTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        panel.realize(root_window(), 0, 0);
        panel.map();
    }

    // the redraw is asked for, the events must be handled
    void draw()
    {
        XSync(tiny::get_display(), False);
        pump_events();
    }

    uint32_t pixel(int x, int y)
    {
        draw();
        return Image(panel.get_window(), panel.get_width(), panel.get_height())
                .at(x, y);
    }

    // the cell `i` of the shown ones, a point in its corner
    int cell_x(size_t i) { return border() + static_cast<int>(i) * cell + 2; }

    int cell_y() { return border() + 2; }

    WidgetFont font;

    const ShellFonts shell_fonts;
    SwitcherPanel panel{icon, cell};
};

TEST_F(SwitcherPanelTest, is_a_window_without_a_border)
{
    EXPECT_EQ(attributes_of(panel).border_width, 0);
    EXPECT_EQ(&panel.get_style(), &tiny::theme.list);
}

TEST_F(SwitcherPanelTest, is_as_wide_as_its_cells_and_the_margin)
{
    panel.set_cells(cells_of(3), 0);

    EXPECT_EQ(panel.get_width(), 3 * cell + 2 * border());
    EXPECT_EQ(panel.get_width(), panel.width_of(3));
    EXPECT_EQ(attributes_of(panel).width, panel.get_width());
}

TEST_F(SwitcherPanelTest, is_high_enough_for_an_icon_dots_and_a_text)
{
    EXPECT_GT(panel.get_cell_height(), icon + tiny::wm::shell_theme.dock_dot);
    EXPECT_EQ(panel.get_height(), panel.get_cell_height() + 2 * border());
}

TEST_F(SwitcherPanelTest, the_cell_with_the_cursor_has_another_background)
{
    panel.set_cells(cells_of(3), 1);

    EXPECT_EQ(pixel(cell_x(1), cell_y()), background(true));
    EXPECT_EQ(pixel(cell_x(0), cell_y()), background(false));
    EXPECT_EQ(pixel(cell_x(2), cell_y()), background(false));
}

TEST_F(SwitcherPanelTest, the_cursor_moves)
{
    panel.set_cells(cells_of(3), 1);

    panel.select(2);

    EXPECT_EQ(panel.get_selected(), 2u);
    EXPECT_EQ(pixel(cell_x(2), cell_y()), background(true));
    EXPECT_EQ(pixel(cell_x(1), cell_y()), background(false));
}

TEST_F(SwitcherPanelTest, no_cursor_marks_no_cell)
{
    panel.set_cells(cells_of(2), std::nullopt);

    EXPECT_EQ(panel.get_selected(), std::nullopt);
    EXPECT_EQ(pixel(cell_x(0), cell_y()), background(false));
    EXPECT_EQ(pixel(cell_x(1), cell_y()), background(false));
}

TEST_F(SwitcherPanelTest, only_the_cells_that_fit_are_shown_around_the_cursor)
{
    panel.set_max_width(3 * cell + 2 * border());

    panel.set_cells(cells_of(10), 5);

    const auto visible = panel.get_visible();
    EXPECT_EQ(visible.count, 3u);
    EXPECT_EQ(visible.first, 4u);
    EXPECT_EQ(panel.get_width(), 3 * cell + 2 * border());
    EXPECT_EQ(pixel(cell_x(1), cell_y()), background(true)); // the middle one
}

TEST_F(SwitcherPanelTest, the_shown_cells_follow_the_cursor)
{
    panel.set_max_width(2 * cell + 2 * border());
    panel.set_cells(cells_of(6), 0);

    panel.select(5);

    EXPECT_EQ(panel.get_visible().first, 4u);
}

TEST_F(SwitcherPanelTest, an_icon_is_drawn_in_the_size_of_the_panel)
{
    tiny::image::Image red;
    red.width = 16;
    red.height = 16;
    red.pixels.assign(16 * 16, 0xFFFF0000);
    const Pixmap pixmap =
            tiny::image::create_pixmap(tiny::get_display(), root_window(), red);
    const Picture picture =
            tiny::image::create_argb_picture(tiny::get_display(), pixmap);
    auto list = cells_of(2);
    list[0].picture = picture;
    list[0].icon_width = 16;
    list[0].icon_height = 16;

    panel.set_cells(list, 1);

    const int top = border() + border(); // the margin and the gap of the cell
    // the middle of the icon is red, a corner of the cell is not
    EXPECT_EQ(pixel(border() + cell / 2, top + icon / 2), 0xFF0000u);
    EXPECT_EQ(pixel(border() + cell / 2 + icon / 2 - 1, top + icon - 2),
              0xFF0000u); // scaled up to the whole size
    EXPECT_NE(pixel(border() + 1, top + icon / 2), 0xFF0000u);
    XRenderFreePicture(tiny::get_display(), picture);
    XFreePixmap(tiny::get_display(), pixmap);
}

TEST_F(SwitcherPanelTest, dots_show_the_windows_of_an_application)
{
    auto list = cells_of(2);
    list[0].dots = 3;
    list[1].dots = 1; // one window has no dot
    panel.set_cells(list, 1);

    const int dot = tiny::wm::shell_theme.dock_dot;
    const int y = border() + border() + icon + border() / 2 + dot / 2;
    const uint32_t fg = tiny::theme.list.get_fg(
            static_cast<uint8_t>(tiny::Style::State::Normal));
    const int step = 2 * dot;
    const int left = border() + (cell - (3 * step - dot)) / 2;
    EXPECT_EQ(pixel(left + dot / 2, y), fg);
    EXPECT_EQ(pixel(left + step + dot / 2, y), fg);
    EXPECT_EQ(pixel(left + 2 * step + dot / 2, y), fg);
    EXPECT_NE(pixel(left + dot + dot / 2, y), fg);       // between the dots
    EXPECT_NE(pixel(border() + cell + cell / 2, y), fg); // one window
}

TEST_F(SwitcherPanelTest, the_text_is_drawn_under_the_icon)
{
    panel.set_cells(cells_of(1), std::nullopt);
    draw();
    const Image image(panel.get_window(), panel.get_width(),
                      panel.get_height());

    // the rows of the text have pixels of another color than the background
    unsigned int text = 0;
    for (int y = panel.get_height() - panel.get_cell_height() / 3;
         y < panel.get_height() - border(); ++y) {
        for (int x = 0; x < panel.get_width(); ++x) {
            text += image.at(x, y) != background(false);
        }
    }
    EXPECT_GT(text, 10u);
}

TEST_F(SwitcherPanelTest, a_long_text_does_not_leave_the_cell)
{
    auto list = cells_of(2);
    list[0].text = std::string(200, 'W');
    panel.set_cells(list, std::nullopt);
    draw();
    const Image image(panel.get_window(), panel.get_width(),
                      panel.get_height());

    const int y = panel.get_height() - border() - 4;
    for (int x = border() + cell; x < border() + cell + 2; ++x) {
        EXPECT_EQ(image.at(x, y), background(false)) << x; // between the cells
    }
}

TEST_F(SwitcherPanelTest, an_application_without_an_icon_has_the_generic_one)
{
    const IconFiles files{"application-x-executable"};
    tiny::wm::SymbolicIcons icons(tiny::get_display(), root_window(),
                                  {files.dir()});
    panel.set_icons(&icons);

    panel.set_cells(cells_of(1), std::nullopt);

    const int top = border() + border();
    EXPECT_NE(pixel(border() + cell / 2, top + icon / 2), background(false));
}

TEST_F(SwitcherPanelTest, the_old_icon_of_a_window_is_copied)
{
    const Pixmap pixmap =
            XCreatePixmap(tiny::get_display(), root_window(),
                          tiny::x_uint(tiny::wm::shell_theme.icon),
                          tiny::x_uint(tiny::wm::shell_theme.icon),
                          tiny::x_uint(XDefaultDepth(tiny::get_display(), 0)));
    GC gc = XCreateGC(tiny::get_display(), pixmap, 0, nullptr);
    XSetForeground(tiny::get_display(), gc, 0x00FF00);
    XFillRectangle(tiny::get_display(), pixmap, gc, 0, 0,
                   tiny::x_uint(tiny::wm::shell_theme.icon),
                   tiny::x_uint(tiny::wm::shell_theme.icon));
    XFreeGC(tiny::get_display(), gc);
    auto list = cells_of(1);
    list[0].pixmap = pixmap;

    panel.set_cells(list, std::nullopt);

    const int top = border() + border();
    EXPECT_EQ(pixel(border() + cell / 2, top + icon / 2), 0x00FF00u);
    XFreePixmap(tiny::get_display(), pixmap);
}

TEST_F(SwitcherPanelTest, the_mask_of_the_old_icon_leaves_the_background)
{
    ::Display* dsp = tiny::get_display();
    const auto size = tiny::x_uint(tiny::wm::shell_theme.icon);
    const Pixmap pixmap = XCreatePixmap(dsp, root_window(), size, size,
                                        tiny::x_uint(XDefaultDepth(dsp, 0)));
    GC gc = XCreateGC(dsp, pixmap, 0, nullptr);
    XSetForeground(dsp, gc, 0x00FF00);
    XFillRectangle(dsp, pixmap, gc, 0, 0, size, size);
    XFreeGC(dsp, gc);
    // a mask with no bit set: nothing of the icon is shown
    const Pixmap mask = XCreatePixmap(dsp, root_window(), size, size, 1);
    GC mask_gc = XCreateGC(dsp, mask, 0, nullptr);
    XSetForeground(dsp, mask_gc, 0);
    XFillRectangle(dsp, mask, mask_gc, 0, 0, size, size);
    XFreeGC(dsp, mask_gc);
    auto list = cells_of(1);
    list[0].pixmap = pixmap;
    list[0].mask = mask;

    panel.set_cells(list, std::nullopt);

    const int top = border() + border();
    EXPECT_EQ(pixel(border() + cell / 2, top + icon / 2), background(false));
    XFreePixmap(dsp, pixmap);
    XFreePixmap(dsp, mask);
}

TEST_F(SwitcherPanelTest, is_put_with_its_middle_at_a_point)
{
    panel.set_cells(cells_of(2), 0);

    panel.place(500, 40);

    EXPECT_EQ(attributes_of(panel).x, 500 - panel.get_width() / 2);
    EXPECT_EQ(attributes_of(panel).y, 40);
}

TEST_F(SwitcherPanelTest, a_new_size_of_the_cells_resizes_the_window)
{
    panel.set_cells(cells_of(2), 0);

    panel.set_geometry(64, 120);

    EXPECT_EQ(panel.get_cell_width(), 120);
    EXPECT_EQ(panel.get_width(), 2 * 120 + 2 * border());
    EXPECT_EQ(attributes_of(panel).width, panel.get_width());
}

TEST(SwitcherPanelUnrealizedTest, does_not_draw_or_move_before_it_is_realized)
{
    SwitcherPanel panel(icon, cell);

    panel.set_cells(cells_of(2), 1);
    panel.select(0);
    panel.place(10, 10);
    panel.set_geometry(icon, cell);

    EXPECT_EQ(panel.get_count(), 2u);
    EXPECT_EQ(panel.get_width(), 2 * cell + 2 * border());
}
