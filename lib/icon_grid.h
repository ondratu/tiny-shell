#pragma once

#include <X11/extensions/Xrender.h>

#include <cstddef>
#include <functional>
#include <string>

#include "list_box.h"
#include "widget.h"

namespace tiny {

/**
 * @short Cells of an icon above a text in pages, one of them selected.
 *
 * Like the list the grid stores nothing: `item_of` is asked for a cell only
 * when it is drawn. The cells go by rows, `columns` x `rows` of them make a
 * page; the page is the one with the selected cell. Under the cells a dot
 * for every page (the current one is bright) is drawn when there is more
 * than one page. The selection is moved by the owner (the grid does not take
 * the keyboard focus), by the pointer or by the wheel, which turns the pages;
 * a click on a dot goes to its page. on_activate is sent on a click on a
 * cell, the owner reads it with `get_selected()`.
 */
class IconGrid : public Widget {
  public:
    using Item = ListBox::Item;

    using ItemFn = std::function<Item(size_t cell)>;

    /// The height of the widget: the rows and a line of dots.
    static int height_of(int rows_arg, int cell_h, int dot);

    /// The size is `cols` x `rows_arg` cells and a line of dots, `dot` is the
    /// diameter of a dot.
    IconGrid(int cols, int rows_arg, int cell_w, int cell_h, int icon, int dot);

    ~IconGrid() override;

    void set_events(long mask = 0) override;

    const Style& get_style() const override { return theme.list; }

    /// Change the size of the widget and of its cells; the cells stay.
    void set_geometry(int cols, int rows_arg, int cell_w, int cell_h, int icon);

    /// Replace the cells; the selection goes to the first one.
    void set_items(size_t count, ItemFn item_of);

    size_t get_count() const { return count; }

    size_t get_selected() const { return selected; }

    size_t per_page() const { return columns * rows; }

    size_t get_page() const { return selected / per_page(); }

    /// At least one, also for no cells.
    size_t page_count() const;

    /// Select a cell (the last one when it is too big).
    void select(size_t cell);

    void select_next() { select(selected + 1); }

    void select_prev() { select(selected > 0 ? selected - 1 : 0); }

    /// The cell under the current one, if there is any row under it.
    void select_down();

    void select_up();

    /// The same cell on the next or the previous page, if there is one.
    void select_page(bool down);

    /// The cell at the coordinates of the widget, or the count when none.
    size_t cell_at(int x, int y) const;

    /// The page of the dot at the coordinates, or the page count when none.
    size_t dot_at(int x, int y) const;

    Signal on_activate;

    //! The right button was pressed on a cell; the cell is selected
    Signal on_context;

  protected:
    void on_expose(const XEvent& e, void* data);
    void on_motion_notify(const XEvent& e, void* data);
    void on_button_press(const XEvent& e, void* data);
    void on_button_release(const XEvent& e, void* data);

  private:
    void redraw();
    int dots_step() const;
    int dots_left() const;

    ItemFn item_of;
    size_t count = 0;
    size_t selected = 0;
    size_t columns;
    size_t rows;
    int cell_width;
    int cell_height;
    int icon_size;
    int dot_size;
    size_t pressed_cell;
};

} // namespace tiny
