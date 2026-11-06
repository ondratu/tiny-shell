#pragma once

#include <X11/extensions/Xrender.h>

#include <optional>
#include <string>
#include <vector>

#include "wm/shell_theme.h"
#include "wm/switcher_layout.h"
#include "wm/symbolic_icons.h"
#include "widget.h"

namespace tiny::wm {

//! What the switcher shows of an application or a window
struct SwitcherCell {
    std::string text;
    //! The ARGB icon (not owned) of `icon_width` x `icon_height`, drawn in
    //! the size of the panel
    Picture picture = None;
    int icon_width = 0;
    int icon_height = 0;
    //! The icon of the window when there is no `picture`, as the dock draws it
    Pixmap pixmap = None;
    Pixmap mask = None;
    size_t dots = 0; //!< the windows of the application when more than one
};

/**
 * @short A row of icons with a text under each, one of them with the cursor:
 * the look of the dock.
 *
 * An icon has the dots under it (a dot for every window of the application,
 * when there is more than one) and the text under the dots, cut with dots
 * when it does not fit. When the cells do not fit in `set_max_width()` the
 * row shows the ones around the cursor. The panel is a window of its own; the
 * owner puts it on the screen with `place()`.
 */
class SwitcherPanel : public tiny::Widget {
  public:
    //! Cells of `cell_width` with an icon of `icon_size`
    SwitcherPanel(int icon_size, int cell_width);

    ~SwitcherPanel() override;

    int get_border() const override { return 0; }

    //! The look of the list of the launcher, which has a font
    const tiny::Style& get_style() const override { return tiny::theme.list; }

    void set_events(long mask = 0) override;

    //! The icon of an application without one and of a dot; must live longer
    void set_icons(SymbolicIcons* icons_arg) { symbols = icons_arg; }

    //! The icons and cells are made again in these sizes (a new scale)
    void set_geometry(int icon_size, int cell_width);

    //! The widest the panel can be; the cells that do not fit are left out
    void set_max_width(int value);

    //! Replace the cells; the cursor is on `selected` (none: nowhere)
    void set_cells(std::vector<SwitcherCell> list,
                   std::optional<size_t> selected);

    //! Move the cursor
    void select(std::optional<size_t> cell);

    size_t get_count() const { return cells.size(); }

    std::optional<size_t> get_selected() const { return selected; }

    //! The cells that are shown
    CellRange get_visible() const;

    int get_cell_width() const { return cell_width; }

    int get_cell_height() const;

    //! The size the panel has with these cells (the server tells it later)
    int width_of(size_t cell_count) const;

    //! Put the panel with its middle at `x` and the top at `y`, in its size
    void place(int x_center, int y);

    //! Draw the cells again
    void redraw();

  private:
    void on_expose(const XEvent& e);
    void draw_icon(const SwitcherCell& cell, int x, int y);
    void draw_dots(GC gc, const SwitcherCell& cell, int x, int y);
    //! Change the size of the window to the cells that are shown
    void fit_window();

    int icon_size;
    int cell_width;
    int max_width = 0; //!< 0: no limit
    std::vector<SwitcherCell> cells;
    std::optional<size_t> selected;
    SymbolicIcons* symbols = nullptr;
    Picture target = None;
};

} // namespace tiny::wm
