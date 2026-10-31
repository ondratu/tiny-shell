#pragma once

#include <X11/extensions/Xrender.h>

#include <cstddef>
#include <functional>
#include <string>

#include "widget.h"

namespace tiny {

/**
 * @short Rows of an icon and a text, one of them selected.
 *
 * The rows are not stored: the widget asks `item_of` for a row only when it
 * draws it, so a list of thousands of rows costs nothing and the icons are
 * made just for the visible rows. The selection is moved by the owner (the
 * list does not take the keyboard focus, a text field above it does) or by
 * the pointer, and the wheel scrolls the rows (the selection stays in view);
 * on_activate is sent on a click, the owner reads the row with
 * `get_selected()`.
 */
class ListBox : public Widget {
  public:
    struct Item {
        std::string text;
        Picture icon = None; //!< not owned; None draws no icon
        int icon_width = 0;  //!< 0 is the icon size of the list
        int icon_height = 0;
    };

    using ItemFn = std::function<Item(size_t row)>;

    ListBox(int width, int height, int row_height, int icon_size);

    ~ListBox() override;

    void set_events(long mask = 0) override;

    const Style& get_style() const override { return theme.list; }

    /// Replace the rows; the selection and the scrolling go to the first row.
    void set_items(size_t count, ItemFn item_of);

    size_t get_count() const { return count; }

    size_t get_selected() const { return selected; }

    size_t get_first() const { return first; }

    /// How many rows fit in the widget (at least one).
    size_t visible_rows() const;

    /// Select a row (the last one when it is too big) and scroll to it.
    void select(size_t row);

    void select_next() { select(selected + 1); }

    void select_prev() { select(selected > 0 ? selected - 1 : 0); }

    void select_page(bool down);

    /// The row at the y coordinate of the widget, or the count when none.
    size_t row_at(int y) const;

    Signal on_activate;

  protected:
    void on_expose(const XEvent& e, void* data);
    void on_motion_notify(const XEvent& e, void* data);
    void on_button_press(const XEvent& e, void* data);
    void on_button_release(const XEvent& e, void* data);

  private:
    void redraw();
    void scroll_to(size_t new_first);

    ItemFn item_of;
    size_t count = 0;
    size_t selected = 0;
    size_t first = 0;
    int row_height;
    int icon_size;
    size_t pressed_row;
};

} // namespace tiny
