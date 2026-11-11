#pragma once

#include <X11/Xlib.h>

namespace tiny::wm {

/**
 * @short A black window over the whole screen: the display "switched off" by
 * software.
 *
 * It is for the X servers where DPMS does nothing (a virtual machine, some
 * drivers) and as the picture of a display that is off on the others. The
 * window takes no input: whoever shows it looks at the idle time and hides it
 * with the first key or move of the pointer.
 */
class ScreenBlank {
  public:
    explicit ScreenBlank(::Display* display_arg): display(display_arg) {}
    ScreenBlank(const ScreenBlank&) = delete;
    ScreenBlank& operator=(const ScreenBlank&) = delete;
    ~ScreenBlank();

    //! Cover the screen (the window is made at the first call)
    void show();

    //! Uncover it
    void hide();

    bool shown() const { return visible; }

    //! The window, `None` before the first `show()`
    ::Window get_window() const { return window; }

  private:
    ::Display* display;
    ::Window window = None;
    Cursor cursor = None; //!< no pointer to see on a black screen
    bool visible = false;
};

} // namespace tiny::wm
