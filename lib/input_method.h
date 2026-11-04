#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <string>

namespace tiny {

/// Latin-1 bytes (what `XLookupString` gives without an input method) as UTF-8.
std::string latin1_to_utf8(const char* bytes, int len);

/**
 * @short The input method and the input context of one window.
 *
 * An input method gives the characters that are not Latin-1; without one
 * (`available()` is false) the keys give Latin-1 only, which `lookup()`
 * converts to UTF-8 the same way. Shared by the entry and the programs that
 * draw their own text fields.
 */
class InputMethod {
  public:
    InputMethod(::Display* display, ::Window window);
    ~InputMethod();
    InputMethod(const InputMethod&) = delete;
    InputMethod& operator=(const InputMethod&) = delete;

    bool available() const { return xic != nullptr; }

    //! The typed characters of a key in UTF-8 (empty for a key without
    //! characters); sets the keysym of the key
    std::string lookup(const XKeyEvent& key, KeySym& keysym) const;

    //! Tell the input method that the window has or has not the focus
    void set_focus(bool focus) const;

  private:
    XIM xim = nullptr;
    XIC xic = nullptr;
};

} // namespace tiny
