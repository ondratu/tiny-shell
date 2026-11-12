# Features

What is implemented. What is planned is in the [roadmap](roadmap.md), and a
comparison with other window managers is in the [comparison](comparison.md).

- **Window manager:** reparenting frames drawn by tiny-shell, with a title bar
  (the title and the minimize, maximize and close buttons), a border and edges to resize by.
  - Move and resize with the pointer (the title bar, the edges and the
    corners), maximize, fullscreen (F11), minimize, shade (a double click on
    the title bar), dialogs and modal dialogs, a window switcher.
  - The **window menu** (see [the window menu](usage.md#the-window-menu)) for everything that has no button.
  - Windows that were there before the manager started are adopted, and clients
    survive a crash of the manager and are given back when it exits.
  - Applications that stop responding are found by `_NET_WM_PING`, and the user
    chooses to wait or to force them to quit.
  - A window that supports `_NET_WM_SYNC_REQUEST` is not given a new size by the
    edges before it has drawn the last one.
- **Standards:** programs need to know nothing about tiny-shell.
  - ICCCM and EWMH: window states, types and layers; struts and a work area that
    follows docks coming and going; the active window and focus stealing
    prevention; frame extents; the client lists; the requests of a client to
    move, resize and restack its window (`_NET_WM_MOVERESIZE`,
    `_NET_MOVERESIZE_WINDOW`, `_NET_RESTACK_WINDOW`) and to show its window menu
    (`_GTK_SHOW_WINDOW_MENU`); fullscreen over several
    monitors (`_NET_WM_FULLSCREEN_MONITORS`); show desktop
    (`_NET_SHOWING_DESKTOP`) and the name of the desktop; the geometry of the
    dock icon of a window (`_NET_WM_ICON_GEOMETRY`). Menus, tooltips,
    notifications, combo lists and drag pictures get no frame and no focus and
    stay above the windows.
  - Startup notification: the pointer is busy while an application starts (see
    [Using the shell](usage.md#startup-notification)).
  - GTK windows that draw their own title bar (`_GTK_FRAME_EXTENTS`) are placed
    by their visible part, not by their shadow.
  - XSETTINGS (the theme, the font and the DPI for GTK and Qt, see [Configuration](configuration.md#settings-for-gtk-and-qt-xsettings)),
    `_MOTIF_WM_HINTS` (functions and decorations), Desktop Entry and the Icon
    Theme specification (PNG, SVG, XPM).
- **Shell:** top panel with the logo, which opens the main menu (About,
  Settings, Applications, Terminal, Force Quit, Restart, Shutdown, Lock Screen,
  Logout), the
  date and time at its right end (the names and the order follow `LANG`), the
  system tray (the icons of programs, left of the clock), a dock with one icon
  for every application (a dot under it for every window, also for an
  application that is not a favorite) and the favorite applications, which stay
  there when they do not run and can be moved by the mouse; the dock hides under
  the active window and the bottom edge of the screen shows it (or it is always
  shown, or never); a run dialog (Alt+F2, Tab completes the name of a program
  from `PATH`: the first Tab adds what all the matching programs share, the next
  ones walk through them), a full screen launcher with search (Super). Their
  text field has a cursor (the arrows, Home, End, Delete and a click); Ctrl+V,
  Shift+Insert and the middle button paste, Ctrl+C copies the text.
- **Window switcher:** Alt+Tab shows, while Alt is held, a panel in the middle
  of the screen with an icon and the name of every running application (once,
  with a dot for every window when it has more); the window is switched when Alt
  is released, so the cursor can also stay where it was. The applications are
  ordered by the use of their windows (the most recently used first), so Alt+Tab
  alone goes to the window used before. Tab and Shift+Tab or the left and right
  arrows move the cursor, the down arrow shows the windows of the application
  with big icons and their titles (the left and right arrows choose one, Tab
  still moves between the applications) and the up arrow goes back; Escape
  cancels. Alt+; is the same for the windows of the current application only. It
  is taken only from the keys that type a semicolon without Shift or AltGr (in
  every group of the layout), so Alt with the other keys, also the ones that
  give `;` with a modifier, stays for the applications. The shortcuts of the
  shell (Alt+Tab, Alt+F2, Alt+F4, Alt+Space, Alt+; and Super) follow a change of the
  keyboard layout.
- **Keyboard:** `[keyboard]` sets the layouts (up to four, with their variants)
  and the keys that switch them, the shell applies it with `setxkbmap` at the
  start and on `--reload`; `tiny-settings` has a list of the layouts of the
  system and a list of the shortcuts of the shell.
- **Multiple monitors:** the monitors come from RandR and are numbered from the
  left; the panel, the dock, the launcher and the dialogs are on the primary one
  (chosen in the settings), a window is maximized and made fullscreen on its own
  monitor, and each monitor has its own work area. The secondary monitor can be
  put on one side of the primary one, and the number of every monitor can be
  shown in its corner (see [Configuration](configuration.md#multiple-monitors)).
- **Settings:** `tiny-settings`, a small form with the sections Appearance,
  Fonts, Desktop, Displays, Power, Security, Applications and Session that changes the font, the
  rendering of text (antialiasing, hinting, subpixel order), the scale, the
  wallpaper, the mode of the dock, the format of the clock, the monitors, the
  time to switch the display off, the time to lock the screen after that and the
  terminal in the configuration file of the user (see [Applications](applications.md#settings)).
- **Power saving of the display:** `[display] off_after` switches the display
  off after a time without input, it is Display off in `tiny-settings`. The
  shell covers the screen with a black window and, with DPMS, the X server
  switches the display off as well; `[display] dpms = no` leaves DPMS alone,
  for the drivers where it does nothing or hangs the machine (see
  [Configuration](configuration.md#the-power-saving-of-the-display)).
- **Screen lock:** `tiny-lock` (Super + L, the main menu, or `[lock] delay`
  minutes after the display goes off) asks for the password of the
  system through PAM, a checker such as `unix_chkpwd`, or the small
  `tiny-lock-pwcheck`; only the last one has any privileges. It covers every
  monitor, follows a change of the monitors, goes dark when the display would be off
  and is started again if it dies (see [Screen lock](lock.md)).
- **Wallpaper:** `tiny-wallpaper` draws a PNG or JPEG as the background of the
  root window (fill, center or tile) and publishes it in `_XROOTPMAP_ID`, so a
  transparent terminal can show it; tiny-shell runs it at the start and when the
  screen changes its size (see [Configuration](configuration.md#wallpaper)).
- **Fonts:** `tiny-fonts`, a small program that lists the fonts of the system
  and shows a specimen of each one (see [Applications](applications.md#fonts)).
- **Icons and wallpapers:** a set of symbolic icons (the logo, the buttons of a
  window, the launcher, an application without an icon), the icons and desktop
  files of `tiny-settings` and `tiny-fonts` (so the launcher lists them and the
  dock shows their icons) and wallpapers for the light and the dark style,
  installed by `make install` (see [Installation](install.md#installation)).
- **Look:** two built-in themes, dark (the default) and light, chosen by
  `[theme] style` or in `tiny-settings`, and switchable while the shell runs
  (the actions `theme-dark`, `theme-light` and `theme-toggle`; no key or menu
  item is bound to them yet, see the key bindings in the plan). The theme
  changes everything the shell draws, the title bars of the windows too, and
  the small programs (settings, fonts, about, the lock screen) start in it. The
  panel, the dock, the launcher and the lock screen each have a style of their
  own, `[panel] style`, `[dock] style`, `[launcher] style` and `[lock] style`:
  `auto` follows the theme, `light` and `dark` keep that look in
  any theme (a dark panel and dock over a light desktop, for example).
- **Session:** a session file for display managers and the script
  `tiny-shell-session` that prepares the environment, the programs of
  `~/.config/autostart` (XDG Autostart) start with it (`tiny-settings` lists
  them and switches each on and off), and Logout, Restart and
  Shutdown ask the programs to close first, so that they can ask for the unsaved
  work (see [The session](session.md)).
- **Actions:** the menu items and the shortcuts run actions by name (`logout`,
  `run-dialog`, `launcher`, `next-window`, `show-desktop`, `window-menu`,
  `exec:<command>`...),
  and the panel has slots for small widgets of plugins; this is what the planned
  Lua plugins will use.
- **Other:** one `poll()` event loop for X events, signals and timers (finished
  children are collected by a `SIGCHLD` handler), commands of Restart and
  Shutdown, a configuration file (scale, commands, favorites of the dock),
  taking over from another window manager (`--replace`) and handing over to one,
  Super + mouse moves a window, scaling of the whole theme for high-DPI screens,
  unit tests of the toolkit and the window manager against Xvfb.
