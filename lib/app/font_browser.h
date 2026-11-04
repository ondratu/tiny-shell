#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "widget.h"

namespace tiny::app {

/**
 * @short What the font viewer shows and where, without any drawing.
 *
 * Two views. The overview is a grid of cells, one for every font family, in
 * a column count that fits the width of the window; the cells scroll. The
 * detail is the specimen of one family under a header with a button back;
 * the specimen scrolls. The owner (`tiny-fonts`) tells the size of the window,
 * the height of the specimen (it depends on the metrics of the font, which
 * the browser does not know) and the input; it asks for the rectangles to
 * draw. All the numbers are in pixels.
 *
 * A click is a press and a release on the same thing, so a press that moves
 * away before the release does nothing.
 */
class FontBrowser {
  public:
    enum class View { Overview, Detail };

    enum class Key {
        Left,
        Right,
        Up,
        Down,
        PageUp,
        PageDown,
        Home,
        End,
        Return,
        Back, //!< backspace
        Escape
    };

    //! The sizes of the parts, at the scale 1
    struct Metrics {
        int cell_width = 220;
        int cell_height = 190;
        int margin = 12;            //!< around the cells and the lines
        int preview = 120;          //!< the size of "Aa" in a cell
        int preview_baseline = 135; //!< from the top of a cell
        int name_size = 13;         //!< the font of the names and the header
        int name_baseline = 170;    //!< from the top of a cell
        int header = 40;            //!< the height of the header of the detail
        int back_width = 110;       //!< the width of the back button
        int charset_size = 28;      //!< the lines of the letters and the digits
        int line_gap = 8;           //!< between the lines of the specimen
        int small_size = 8;         //!< the smallest line of text

        //! Every size multiplied by the factor and rounded
        static Metrics scaled(double factor);
    };

    //! A line of the specimen: a text in the family at a size
    struct Line {
        std::string text;
        int size = 0;
    };

    /**
     * The lines of the specimen: the small letters, the capitals, the digits
     * and the signs, then one sentence at 8, 12, 16, 20, 24, 32, 64 and 128
     * pixels (at the scale 1).
     */
    static std::vector<Line> specimen(const Metrics& metrics);

    FontBrowser(std::vector<std::string> families, const Metrics& metrics);

    /// The size of the window; the scrolling is kept inside of the content.
    void resize(int width, int height);

    /// The height of the whole specimen (the lines and the margins).
    void set_detail_height(int height);

    View view() const { return view_; }

    const std::vector<std::string>& families() const { return families_; }

    size_t selected() const { return selected_; }

    /// The selected family, empty when there is none.
    const std::string& family() const;

    int scroll() const { return scroll_; }

    int content_height() const;

    /// Where the scrolling area starts (under the header in the detail).
    int top() const { return view_ == View::Detail ? metrics_.header : 0; }

    /// The height of the scrolling area.
    int view_height() const;

    size_t columns() const { return columns_; }

    /// The cell of a family in the coordinates of the window (scrolled).
    Rect cell_rect(size_t index) const;

    /// The cells that show: from the first one to the one after the last.
    std::pair<size_t, size_t> visible_cells() const;

    /// The cell at the point, or the count of families when there is none.
    size_t cell_at(int x, int y) const;

    /// The button back in the header of the detail.
    Rect back_rect() const;

    /// A button went down at the point.
    void press(int x, int y);

    /// The button went up; opens the cell or goes back when it is the one
    /// that was pressed. True when something changed.
    bool release(int x, int y);

    /// The wheel by notches, positive goes down; true when something changed.
    bool wheel(int notches);

    /// A key; true when something changed.
    bool key(Key key);

    /// Show the detail of a family (nothing for an index out of the list).
    void open(size_t index);

    /// Return to the overview, at the place where it was left.
    void back();

  private:
    int max_scroll() const;
    bool scroll_to(int position);
    bool move_selection(size_t index);
    size_t target_at(int x, int y) const;
    int step() const;
    int grid_left() const;

    std::vector<std::string> families_;
    Metrics metrics_;
    View view_ = View::Overview;
    int width_ = 1;
    int height_ = 1;
    size_t columns_ = 1;
    size_t selected_ = 0;
    int scroll_ = 0;
    int overview_scroll_ = 0;
    int detail_height_ = 0;
    size_t pressed_;
};

} // namespace tiny::app
