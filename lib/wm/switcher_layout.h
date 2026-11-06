#pragma once

#include <cstddef>

namespace tiny::wm {

//! A run of cells: `count` of them from `first`
struct CellRange {
    size_t first = 0;
    size_t count = 0;
};

/**
 * @short The cells of a row that are shown when only `max_cells` fit.
 *
 * All of them when they fit. Otherwise the shown cells are the ones around
 * `selected`, kept inside of the row, so the selected cell is always seen.
 */
CellRange visible_cells(size_t total, size_t selected, size_t max_cells);

//! How many cells of `cell_width` fit in `room`; at least one
size_t cells_that_fit(int room, int cell_width);

} // namespace tiny::wm
