#pragma once

#include <X11/Xlib.h>

#include <functional>
#include <string>

namespace tiny {

//! The text of a single line field from a pasted text: a line break or a tab
//! is a space, the other control characters are dropped
std::string single_line(const std::string& text);

/**
 * @short The text of the X selections of one window: paste and copy.
 *
 * `PRIMARY` is what the middle button pastes, `CLIPBOARD` is what Ctrl+C and
 * Ctrl+V use (ICCCM 2.6, the text goes as `UTF8_STRING`, or `STRING` for old
 * programs). The window gives the three events of the selections to
 * `handle()`. A text that comes in pieces (`INCR`) is not supported.
 */
class Clipboard {
  public:
    enum class Which { Primary, Clipboard };

    //! Gets the text that was asked for with `request()`
    using Receive = std::function<void(const std::string&)>;

    Clipboard(::Display* display, ::Window window);
    ~Clipboard();
    Clipboard(const Clipboard&) = delete;
    Clipboard& operator=(const Clipboard&) = delete;

    //! Ask the owner of the selection for its text; `receive` runs when it
    //! comes (never when there is no text)
    void request(Which which, Receive receive, Time time = CurrentTime);

    //! Become the owner of the selection with this text
    void own(Which which, std::string text, Time time = CurrentTime);

    //! Is the text of the selection ours
    bool owns(Which which) const;

    //! The text we own for the selection (empty when it is not ours)
    const std::string& owned(Which which) const;

    //! Handle `SelectionNotify`, `SelectionRequest` and `SelectionClear`;
    //! false for any other event
    bool handle(const XEvent& e);

  private:
    struct Slot {
        Atom selection = None;
        std::string text;
        bool ours = false;
        Receive receive;
    };

    Slot& slot(Which which)
    {
        return which == Which::Primary ? primary : board;
    }
    const Slot& slot(Which which) const
    {
        return which == Which::Primary ? primary : board;
    }
    Slot* slot_of(Atom selection);

    void on_notify(const XSelectionEvent& e);
    void on_request(const XSelectionRequestEvent& e);

    ::Display* display;
    ::Window window;
    Slot primary;
    Slot board;
};

} // namespace tiny
