#pragma once

#include <X11/Xlib.h>

#include <chrono>
#include <optional>

namespace tiny::wm {

/**
 * @short The time since the last keyboard or pointer input.
 *
 * It reads the counter `IDLETIME` of the SYNC extension (libXext, which the
 * shell links anyway). Xorg has the counter since 2007 (xserver 1.3), as the
 * extension MIT-SCREEN-SAVER has the same number; an X server without it (some
 * remote or embedded ones) makes `available()` false and the idle lock is off.
 */
class IdleTime {
  public:
    explicit IdleTime(::Display* display_arg);

    //! Whether the server has the counter
    bool available() const { return counter != None; }

    //! The time since the last input; empty without the counter or when it
    //! can't be read
    std::optional<std::chrono::milliseconds> idle() const;

  private:
    ::Display* display;
    XID counter = None; //!< the XSyncCounter of IDLETIME
};

} // namespace tiny::wm
