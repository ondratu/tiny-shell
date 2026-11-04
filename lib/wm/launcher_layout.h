#pragma once

#include "wm/shell_theme.h"
#include "widget.h"

namespace tiny::wm {

/**
 * @short Where the parts of the launcher are and how big its icons are.
 *
 * The launcher covers the whole screen. The cells of the icons take at most
 * three quarters of the screen, are centered on it and are at most
 * `wm_launcher_columns` x `wm_launcher_rows` (swapped on a screen that is
 * higher than wide). The size of the icons starts at `wm_launcher_icon` and
 * goes up when the cells would take less than a half of the space in both
 * directions, and down when less than 4 x 2 cells fit (2 x 4 on a high
 * screen), between 16 and 512 pixels. The entry is centered between the
 * panel and the first row, the buttons of the pages are squares in the middle
 * of the free space at the sides of the cells, at the height of their middle.
 */
struct LauncherLayout {
    int icon = 0;
    int columns = 0;
    int rows = 0;
    int cell_width = 0;
    int cell_height = 0;
    Rect screen = {0, 0, 0, 0};
    Rect entry = {0, 0, 0, 0}; //!< without the border
    Rect grid = {0, 0, 0, 0};  //!< with the dots of the pages
    Rect prev = {0, 0, 0, 0};
    Rect next = {0, 0, 0, 0};

    static LauncherLayout compute(int screen_width, int screen_height,
                                  const ShellTheme& theme);

    /// The height above (and under) the cells that is kept for the entry.
    static int band(const ShellTheme& theme);

    bool operator==(const LauncherLayout& other) const;
};

} // namespace tiny::wm
