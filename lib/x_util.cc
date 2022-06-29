#include "x_util.h"

namespace tiny {

void x_grab_key(::Display* display, int keycode, unsigned int modifiers,
        ::Window window)
{
    XGrabKey(display, keycode, modifiers,
             window, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, modifiers|Mod2Mask,  // Num Lock
             window, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, modifiers|LockMask,  // Caps Lock
             window, True, GrabModeAsync, GrabModeAsync);
    // Num Lock + Caps Lock
    XGrabKey(display, keycode, modifiers|Mod2Mask|LockMask,
             window, True, GrabModeAsync, GrabModeAsync);
}

void x_ungrab_key(::Display* display, int keycode, unsigned int modifiers,
        ::Window window)
{
    // Base without Num or Cups locks
    XUngrabKey(display, keycode, modifiers, window);
    // Num Lock
    XUngrabKey(display, keycode, modifiers|Mod2Mask, window);
    // Caps Lock
    XUngrabKey(display, keycode, modifiers|LockMask, window);
    // Num Lock + Caps Lock
    XUngrabKey(display, keycode, modifiers|Mod2Mask|LockMask, window);
}

} // namespace tiny
