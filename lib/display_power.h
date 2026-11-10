#pragma once

#include <X11/Xlib.h>

#include <chrono>
#include <optional>

namespace tiny {

/**
 * @short The power saving of the display (the DPMS extension of the X server).
 *
 * The server counts the time without input and switches the monitor off by
 * itself, so no timer of ours is needed. DPMS is in libXext, which is linked
 * anyway. A server without it (Xvfb, Xephyr, some remote ones) or a display
 * that can't be switched makes `available()` false and every call does
 * nothing.
 */
class DisplayPower {
  public:
    explicit DisplayPower(::Display* display_arg);

    //! Whether the server has DPMS and the display can use it
    bool available() const { return usable; }

    //! Whether the server switches the display off after a time now
    bool enabled() const;

    //! The time without input to switch the display off; empty when DPMS is
    //! off or it can't be read
    std::optional<std::chrono::seconds> timeout() const;

    /**
     * @short Switch the display off after this time without input; zero or
     * less switches DPMS off, so that the display stays on.
     *
     * The levels standby and suspend are not used: the display goes from on
     * to off.
     */
    bool set_timeout(std::chrono::seconds after);

    //! Switch the display off now (only when DPMS is on, else the server
    //! refuses and an error would end the program). Any input switches it on.
    bool off();

    //! Switch the display on now
    bool on();

  private:
    ::Display* display;
    bool usable = false;
};

} // namespace tiny
