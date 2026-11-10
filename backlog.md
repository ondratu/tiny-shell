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

## Background of the lock screen

- **What:** a picture (or a color) behind the time and the dialog of the lock
  screen, e.g. the wallpaper of the shell blurred or dimmed. Now the screen is
  black, which is also what it is after `[lock] blank_after`.
- **Status:** not decided. A setting `[lock] background` (a file, like
  `[wallpaper] file`, or `wallpaper` for the one of the shell) and a field in
  `tiny-settings`. `tiny-lock` has no image decoder: it would have to link the
  one of `lib/image/` (the size of the program, and the memory of a picture in a
  process that holds the keyboard), or the picture is drawn by `tiny-wallpaper`
  into a pixmap that the lock uses.

## Keys of brightness and volume

- **What:** the keys `XF86MonBrightness*` and `XF86Audio*` change the brightness
  and the volume. A key is only a request: the work is done by a command
  (`brightnessctl`, `wpctl`, `amixer`) that a setting names, as `[commands]
  lock` does, so the shell would not link anything new. A small bar can show
  the new value.
- **Status:** not decided. Today a program like `xbindkeys` can do it.

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
