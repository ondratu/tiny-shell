#include <algorithm>
#include <cstdlib>
#include <iterator>

#include "icon_grid.h"
#include "wm/launcher_layout.h"

namespace tiny::wm {

namespace {

constexpr int icon_sizes[] = {16, 24, 32, 48, 64, 96, 128, 256, 512};
constexpr int min_columns = 4;
constexpr int min_rows = 2;

int nearest_size(int icon)
{
    size_t best = 0;
    for (size_t i = 1; i < std::size(icon_sizes); ++i) {
        if (std::abs(icon_sizes[i] - icon) <
            std::abs(icon_sizes[best] - icon)) {
            best = i;
        }
    }
    return static_cast<int>(best);
}

} // namespace

int LauncherLayout::band(const Theme& theme)
{
    return theme.wm_panel + theme.wm_win_header + 2 * theme.entry.border_width +
           2 * theme.wm_win_border;
}

bool LauncherLayout::operator==(const LauncherLayout& other) const
{
    const auto same = [](const Rect& a, const Rect& b) {
        return a.x == b.x && a.y == b.y && a.width == b.width &&
               a.height == b.height;
    };
    return icon == other.icon && columns == other.columns &&
           rows == other.rows && cell_width == other.cell_width &&
           cell_height == other.cell_height && same(screen, other.screen) &&
           same(entry, other.entry) && same(grid, other.grid) &&
           same(prev, other.prev) && same(next, other.next);
}

LauncherLayout LauncherLayout::compute(int screen_width, int screen_height,
                                       const Theme& theme)
{
    const bool high = screen_height > screen_width;
    const int max_columns = std::max(1, high ? theme.wm_launcher_rows
                                             : theme.wm_launcher_columns);
    const int max_rows = std::max(1, high ? theme.wm_launcher_columns
                                          : theme.wm_launcher_rows);
    const int area_width = screen_width * 3 / 4;
    const int area_height =
            std::min(screen_height * 3 / 4, screen_height - 2 * band(theme));

    const auto with_icon = [&](int index) {
        LauncherLayout l;
        l.icon = icon_sizes[index];
        l.cell_width = std::max(2 * l.icon, theme.wm_launcher_cell_width);
        l.cell_height = l.icon + theme.wm_launcher_label;
        l.columns = std::clamp(area_width / l.cell_width, 1, max_columns);
        l.rows = std::clamp(area_height / l.cell_height, 1, max_rows);
        return l;
    };

    int index = nearest_size(theme.wm_launcher_icon);
    while (index > 0) {
        const LauncherLayout l = with_icon(index);
        if (l.columns >= (high ? min_rows : min_columns) &&
            l.rows >= (high ? min_columns : min_rows)) {
            break;
        }
        --index;
    }
    while (index + 1 < static_cast<int>(std::size(icon_sizes))) {
        const LauncherLayout l = with_icon(index);
        if (2 * max_columns * l.cell_width > area_width ||
            2 * max_rows * l.cell_height > area_height) {
            break;
        }
        ++index;
    }

    LauncherLayout l = with_icon(index);
    l.screen = {0, 0, screen_width, screen_height};

    const int cells_width = l.columns * l.cell_width;
    const int cells_height = l.rows * l.cell_height;
    l.grid = {
            (screen_width - cells_width) / 2,
            (screen_height - cells_height) / 2, cells_width,
            IconGrid::height_of(l.rows, l.cell_height, theme.wm_launcher_dot)};

    // a third of the screen, but not too short on a small one
    const int border = theme.entry.border_width;
    const int entry_width = std::max(
            screen_width / 3, std::min(screen_width - 2 * border,
                                       4 * theme.wm_launcher_cell_width));
    const int entry_outer = theme.wm_win_header + 2 * border;
    l.entry = {(screen_width - entry_width) / 2,
               theme.wm_panel + (l.grid.y - theme.wm_panel - entry_outer) / 2,
               entry_width - 2 * border, theme.wm_win_header};

    // squares in the middle of the free space at the sides of the cells
    const int free_left = l.grid.x;
    const int free_right = screen_width - l.grid.x - cells_width;
    const int button = std::max(
            1, std::min({l.cell_width / 2, free_left / 2, free_right / 2}));
    const int button_y = l.grid.y + (cells_height - button) / 2;
    l.prev = {(free_left - button) / 2, button_y, button, button};
    l.next = {l.grid.x + cells_width + (free_right - button) / 2, button_y,
              button, button};
    return l;
}

} // namespace tiny::wm
