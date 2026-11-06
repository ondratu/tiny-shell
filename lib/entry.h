#pragma once

#include <memory>
#include <string>

#include "clipboard.h"
#include "input_method.h"
#include "text_field.h"
#include "widget.h"

namespace tiny {

/**
 * @short Single line text field.
 *
 * The widget only edits the text and tells about it with signals: on_change
 * after every edit, on_activate on Return and on_cancel on Escape. The keys
 * that move in a list or a grid (Up, Down, Page Up, Page Down) are passed to
 * on_key, and Left and Right too after `pass_horizontal_keys()`; else they
 * move the cursor, like Home, End and a click. Tab is told by on_tab (the owner
 * may complete the text). Ctrl+V and Shift+Insert paste
 * the clipboard, the middle button pastes the primary selection, Ctrl+C copies
 * the whole text. What the text means is up to the owner.
 */
class Entry : public Widget {
  public:
    Entry(int width, int height);

    ~Entry() override;

    void realize(Window parent, int x, int y) override;

    void set_events(long mask = 0) override;

    const Style& get_style() const override { return theme.entry; }

    //! Give the keyboard focus to the entry
    void set_focus();

    void set_text(const std::string& text);

    const std::string& get_text() const { return field.text(); }

    void clear();

    //! Give Left and Right to on_key (a grid next to the entry uses them)
    //! instead of moving the cursor
    void pass_horizontal_keys(bool pass = true) { pass_horizontal = pass; }

    //! The place of the cursor: the number of bytes before it
    size_t get_cursor() const { return field.cursor(); }

    Signal on_change;
    Signal on_activate;
    Signal on_cancel;
    Signal on_key;
    Signal on_tab;

  protected:
    void on_key_press(const XEvent& e, void* data);
    void on_button_press(const XEvent& e, void* data);
    void on_expose(const XEvent& e, void* data);
    void on_focus_in(const XEvent& e, void* data);
    void on_focus_out(const XEvent& e, void* data);

  private:
    void redraw();
    //! Where the text starts: it scrolls to keep the cursor visible
    int text_origin(XftFont* font) const;
    int text_width(XftFont* font, const std::string& text) const;
    void paste(Clipboard::Which which, const XEvent& e);
    void set_focus_state(bool focus, const XEvent& e);

    TextField field; //!< the text and the cursor in it
    bool pass_horizontal = false;

    std::unique_ptr<InputMethod> input;   //!< made when the entry is realized
    std::unique_ptr<Clipboard> clipboard; //!< the same
};

} // namespace tiny
