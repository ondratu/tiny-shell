# Backlog

Ideas that were not judged yet. Not to be mixed with todo.txt (the work) and
wont.md (what was judged and left out on purpose).

## Focus follows mouse

- **What:** an option that gives the keyboard focus to the window under the
  pointer, without a click (click to focus stays the default). GNOME has it as
  the hidden key `org.gnome.desktop.wm.preferences focus-mode`
  (`click`, `sloppy`, `mouse`).
- **Status:** not decided if it will be written at all. Lowest priority.
- **Notes:** `EnterNotify` on the frame calls a variant of
  `Manager::focus_window()` without the raise. Care is needed for modal
  dialogs, the dock and menus (no focus taken), and for `EnterNotify` that the
  user did not cause (a window that appears under a still pointer,
  `NotifyGrab`/`NotifyUngrab`). It needs a setting in the ini file and in
  `tiny-settings`.
- **Mentioned in:** `doc/comparison.md` (the symbol `?`).

## Screen lock

- **What:** lock the screen (a key, the main menu, before sleep) and ask for a
  password. LXDE, Xfce and GNOME have it, the plain window managers use an
  external locker (`i3lock`, `slock`, `xscreensaver`).
- **Status:** not decided (`?` in `doc/comparison.md`). The simplest way is to start a configured command
  (`[commands] lock`) and to show a lock item in the main menu, so tiny-shell
  does not read passwords. A locker of its own is a security feature that
  needs a careful design. `TODO-DBUS` has the logind side (lock before sleep).

## Power saving of the display (DPMS) and idle

- **What:** switch the display off after a time without input (`DPMS`
  extension, `XSetScreenSaver`), maybe a setting in `tiny-settings` and the
  keys of brightness and volume (`XF86*`).
- **Status:** not decided. Today the X server defaults apply and `xset` works.

## Accessibility

- **What:** sticky and slow keys, high contrast, a larger cursor, a magnifier;
  the keyboard way to everything in the shell (see "Window menu" in `TODO`).
- **Status:** not decided. A screen reader (Orca, AT-SPI) works with any
  window manager that follows EWMH and does not need the shell, but tiny-shell
  does not publish accessibility on its own widgets.

## File manager

- **What:** a very small file manager of tiny-shell (browse, open with the
  associated application, copy, move, delete), as another small program like
  `tiny-fonts`. LXDE (PCManFM), Xfce (Thunar) and GNOME (Files) have one, the
  plain window managers do not.
- **Status:** not decided (`?` in `doc/comparison.md`). The README says it is
  not a desktop environment; if it comes, it stays tiny, with no desktop icons.

## Screenshot tool

- **What:** a small program (`tiny-screenshot`) that saves the screen, a
  window or an area to a PNG (`XGetImage`, a PNG encoder that the project
  already has or a small optional one), maybe with the Print key as an action.
  Xfce and GNOME have it, the plain window managers use `scrot` or `maim`.
- **Status:** not decided (`?` in `doc/comparison.md`). The scripts of the
  screenshots for the documentation could use it.
