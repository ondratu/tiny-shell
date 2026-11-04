#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "widget.h"

namespace tiny {

/**
 * @short What the settings program shows and does, without any drawing.
 *
 * The form has three text fields (the font, the terminal, the scale), a
 * button that opens the list of fonts, and the buttons Cancel and Save. The
 * list of fonts replaces the form: a search field above the families, one
 * family selected; a click or Return takes the family into the form, Escape
 * goes back and leaves the font as it was. The owner (`tiny-settings`) tells
 * the size of the window and the input, asks for the rectangles to draw and
 * writes the file when `result()` says Saved. All the numbers are in pixels.
 *
 * A click is a press and a release on the same thing, so a press that moves
 * away before the release does nothing.
 */
class SettingsForm {
  public:
    enum class View { Form, Picker };

    enum class Field { Font, Terminal, Scale };

    enum class Key {
        Up,
        Down,
        PageUp,
        PageDown,
        Home,
        End,
        Return,
        Back, //!< backspace
        Escape,
        Tab,
        BackTab //!< Shift+Tab
    };

    //! Editing goes on until the user saves (Saved) or gives up (Cancelled)
    enum class Result { Editing, Saved, Cancelled };

    //! The texts of the fields, as the user sees them
    struct Values {
        std::string font;
        std::string terminal;
        std::string scale;
    };

    //! The sizes of the parts, at the scale 1
    struct Metrics {
        int margin = 16;
        int label_width = 90;   //!< the labels left of the fields
        int row_height = 34;    //!< a field, a button, a row of the list
        int row_gap = 14;       //!< between the fields
        int button_width = 100; //!< Save and Cancel
        int choose_width = 120; //!< the button that opens the list of fonts
        int text_size = 15;     //!< the font of the text
        int scroll_step = 3;    //!< the rows of one notch of the wheel

        //! Every size multiplied by the factor and rounded
        static Metrics scaled(double factor);
    };

    SettingsForm(Values initial, std::vector<std::string> families,
                 const Metrics& metrics);

    /// The size of the window.
    void resize(int width, int height);

    /// The height that shows the form without scrolling.
    int form_height() const;

    View view() const { return view_; }

    Result result() const { return result_; }

    Field focus() const { return focus_; }

    const Values& values() const { return values_; }

    /// The text of a field.
    const std::string& text(Field field) const;

    /// What is wrong in a field, empty when the text can be saved.
    std::string problem(Field field) const;

    /// What is wrong in the first field that has a problem; empty when none.
    std::string problem() const;

    /// The values to write: the texts of the fields without the blanks at both
    /// ends.
    Values clean_values() const;

    /// The message under the fields (a failed write); cleared by an edit.
    const std::string& status() const { return status_; }

    /// The write failed: editing goes on and `message` is shown.
    void reject(const std::string& message);

    // The form

    Rect label_rect(Field field) const;
    Rect field_rect(Field field) const;
    Rect choose_rect() const;
    Rect cancel_rect() const;
    Rect save_rect() const;

    /// Where the hint, and under it the problem, are written.
    Rect message_rect() const;

    // The list of fonts

    const std::string& query() const { return query_; }

    /// The families that match the search.
    const std::vector<std::string>& families() const { return shown_; }

    /// The index in `families()` of the selected one.
    size_t selected() const { return selected_; }

    /// The first row that shows.
    size_t first() const { return first_; }

    /// How many rows fit under the search field (at least one).
    size_t visible_rows() const;

    Rect search_rect() const;

    /// The row of the list in the coordinates of the window, for `first()` + n.
    Rect row_rect(size_t index) const;

    /// A button went down at the point.
    void press(int x, int y);

    /// The button went up; acts when it is on the thing that was pressed.
    /// True when something changed.
    bool release(int x, int y);

    /// The wheel by notches, positive goes down; true when something changed.
    bool wheel(int notches);

    /// A key; true when something changed.
    bool key(Key pressed);

    /// A text that was typed (in UTF-8): it goes to the focused field or to
    /// the search; true when something changed.
    bool type(const std::string& typed);

    /// Show the list of fonts, the family of the font field selected.
    void open_picker();

    /// Take the selected family into the font field and show the form.
    void choose();

    /// Show the form again, the font is as it was.
    void close_picker();

  private:
    enum Target : size_t {
        NoTarget,
        FontField,
        TerminalField,
        ScaleField,
        ChooseButton,
        CancelButton,
        SaveButton,
        FirstRow //!< and the rows of the list from here
    };

    size_t target_at(int x, int y) const;
    bool act(size_t target);
    bool request_save();
    std::string& focused_text();
    bool edit(const std::string& added);
    bool picker_key(Key pressed);
    bool select(size_t index);
    void search(const std::string& query);
    void reveal();
    size_t max_first() const;
    int rows_top() const;

    Values values_;
    std::vector<std::string> all_;
    std::vector<std::string> shown_;
    std::string query_;
    std::string status_;
    Metrics metrics_;
    View view_ = View::Form;
    Result result_ = Result::Editing;
    Field focus_ = Field::Font;
    int width_ = 1;
    int height_ = 1;
    size_t selected_ = 0;
    size_t first_ = 0;
    size_t pressed_ = NoTarget;
};

} // namespace tiny
