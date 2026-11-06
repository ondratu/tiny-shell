#include "wm/switcher_layout.h"

#include <algorithm>

namespace tiny::wm {

CellRange visible_cells(size_t total, size_t selected, size_t max_cells)
{
    max_cells = std::max<size_t>(1, max_cells);
    if (total <= max_cells) {
        return {0, total};
    }
    selected = std::min(selected, total - 1);
    // the selected cell in the middle, the row kept between its ends
    const size_t before = (max_cells - 1) / 2;
    const size_t first = selected > before ? selected - before : 0;
    return {std::min(first, total - max_cells), max_cells};
}

size_t cells_that_fit(int room, int cell_width)
{
    if (room <= 0 || cell_width <= 0) {
        return 1;
    }
    return std::max<size_t>(1, static_cast<size_t>(room / cell_width));
}

} // namespace tiny::wm
