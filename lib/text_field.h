#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace tiny {

/**
 * @short The text of a single line field and the place of the cursor in it.
 *
 * The cursor is a byte offset that is always at the start of a UTF-8
 * character, so a move or a delete takes whole characters. It knows nothing
 * about drawing; the owner gives the widths of the text where the pointer
 * has to be turned into a place (`offset_at`).
 */
class TextField {
  public:
    /// Measures the width of a text in pixels.
    using Measure = std::function<int(const std::string&)>;

    explicit TextField(std::string text = "");

    const std::string& text() const { return text_; }

    /// Where the cursor is: the number of bytes before it.
    size_t cursor() const { return cursor_; }

    /// Replace the text; the cursor goes to its end.
    void set_text(std::string text);

    /// Make room for `size` bytes, so that a text that grows does not move
    /// to a new buffer and leave a copy of itself in the old one.
    void reserve(size_t size) { text_.reserve(size); }

    /// Empty the text and overwrite the bytes it was kept in, also the ones
    /// that an edit left behind the end (a password must not stay in memory).
    void wipe();

    /// Put a text at the cursor; true when there was something to put.
    bool insert(const std::string& added);

    /// Delete the character before the cursor (Backspace); true when it was
    /// there.
    bool backspace();

    /// Delete the character after the cursor (Delete); true when it was there.
    bool erase();

    // Moves of the cursor; true when the cursor moved.
    bool left();
    bool right();
    bool home();
    bool end();

    /// Put the cursor at a byte offset, moved back to the start of its
    /// character when it is inside of one.
    void set_cursor(size_t offset);

    /**
     * @short The place for a pointer at `x`, in pixels from the start of the
     * text: the border of the characters that is the nearest one.
     *
     * Without `measure` it is the end of the text.
     */
    size_t offset_at(int x, const Measure& measure) const;

  private:
    //! The start of the character that ends at `offset`
    size_t previous(size_t offset) const;
    //! The start of the character that follows the one at `offset`
    size_t next(size_t offset) const;

    std::string text_;
    size_t cursor_ = 0;
};

} // namespace tiny
