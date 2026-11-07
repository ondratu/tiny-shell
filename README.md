# tiny-shell

A very small and light window manager and desktop shell for X11, written
directly on top of Xlib.

*tiny-shell* is meant to be a tiny alternative to GNOME Shell: a reparenting
window manager with a panel, a dock, a menu and a launcher, in a few thousand
lines of C++ that depend on nothing but libX11 and a couple of light X
libraries. It targets embedded Linux systems that run Xorg, and it is also a
proof of concept: a way to learn Xlib properly and to squeeze out of it as much
as it can give.

> **Status:** a working prototype. It manages, decorates, moves, resizes,
> minimizes, maximizes and closes windows, and has a panel, a dock, a run
> dialog, an application launcher and a clock. A task bar is still missing, see
> below.

## What it is

- A reparenting window manager with title bars, buttons and borders drawn by
  tiny-shell itself.
- A small shell around it: top panel with the main menu, a dock with the icons
  of running windows, a run dialog.
- A set of freedesktop.org standards implemented as faithfully as possible
  (ICCCM, EWMH, Desktop Entry, Icon Theme), so that ordinary applications just
  work.
- A tiny widget toolkit (`lib/`, the library `libtiny-widgets.a`), made of plain
  X windows and Xft text, loosely inspired by GTK. It exists to serve the shell,
  the settings and the font viewer (and later the Lua plugins of the panel), not
  to be reused: it knows nothing about window management.

## What it is not

- Not a desktop environment. No file manager, session manager or desktop icons
  (use an external tool such as idesk if you need them).
- Not a compositor. No transparency, shadows or animations.
- Not a Wayland compositor and not planned to be one. X11 on Xorg only.
- Not a replacement for a full GNOME or KDE session on a desktop computer.
- Not themable to the last pixel. There is one look, which can be scaled
  (`[theme] scale` in the configuration file) and configured within reason.

## Features

Implemented:

- **Window manager:** reparenting frames drawn by tiny-shell, with a title bar
  (the window menu button, the title, and the minimize, maximize and close
  buttons), a border and edges to resize by.
  - Move and resize with the pointer (the title bar, the edges and the
    corners), maximize, fullscreen (F11), minimize, shade (a double click on
    the title bar), dialogs and modal dialogs, a window switcher.
  - The **window menu** (see below) for everything that has no button.
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
    below).
  - GTK windows that draw their own title bar (`_GTK_FRAME_EXTENTS`) are placed
    by their visible part, not by their shadow.
  - XSETTINGS (the theme, the font and the DPI for GTK and Qt, see below),
    `_MOTIF_WM_HINTS` (functions and decorations), Desktop Entry and the Icon
    Theme specification (PNG, SVG, XPM).
- **Shell:** top panel with the logo, which opens the main menu (About,
  Settings, Applications, Terminal, Force Quit, Restart, Shutdown, Logout), the
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
- **Multiple monitors:** the monitors come from RandR and are numbered from the
  left; the panel, the dock, the launcher and the dialogs are on the primary one
  (chosen in the settings), a window is maximized and made fullscreen on its own
  monitor, and each monitor has its own work area. The secondary monitor can be
  put on one side of the primary one, and the number of every monitor can be
  shown in its corner (see below).
- **Settings:** `tiny-settings`, a small form with the sections Fonts,
  Appearance, Applications and Screens that changes the font, the rendering of
  text (antialiasing, hinting, subpixel order), the scale, the wallpaper, the
  mode of the dock, the format of the clock, the terminal and the monitors in
  the configuration file of the user (see below).
- **Wallpaper:** `tiny-wallpaper` draws a PNG or JPEG as the background of the
  root window (fill, center or tile) and publishes it in `_XROOTPMAP_ID`, so a
  transparent terminal can show it; tiny-shell runs it at the start and when the
  screen changes its size (see below).
- **Fonts:** `tiny-fonts`, a small program that lists the fonts of the system
  and shows a specimen of each one (see below).
- **Icons and wallpapers:** a set of symbolic icons (the logo, the buttons of a
  window, the launcher, an application without an icon), the icons and desktop
  files of `tiny-settings` and `tiny-fonts` (so the launcher lists them and the
  dock shows their icons) and wallpapers for the light and the dark style,
  installed by `make install` (see below).
- **Look:** two built-in themes, dark (the default) and light, chosen by
  `[theme] style` and switchable while the shell runs (the actions `theme-dark`,
  `theme-light` and `theme-toggle`; no key or menu item is bound to them yet,
  see the key bindings in the plan).
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

Planned, grouped by importance (what a window manager must do for other
programs is done, only small details are left):

### Should: what people expect from a desktop

- [ ] Task bar in the panel (the "old style" task list), as an alternative to
      the dock
- [ ] Applications of the launcher grouped by category
- [ ] More in the configuration file: key bindings, panel layout, plugins
- [ ] No flicker: double buffered drawing of the panel and the dock

### Could: nice to have

- [ ] Virtual desktops (there is one desktop now; show desktop works)
- [ ] Plugins and scripting in Lua, as an optional build feature
- [ ] Window snapping to the screen edges and half-screen maximize
- [ ] Session management

## Technology

- Written in C++17 (without RTTI), built with GNU make; two static libraries,
  the widgets and the shell
- [Xlib](https://www.x.org/releases/current/doc/libX11/libX11/libX11.html), Xft
  (with fontconfig and FreeType, which Xft needs anyway), Xrender and Xext (the
  SYNC extension); nothing else is needed to run it
- Tests use [googletest](https://github.com/google/googletest) and Xvfb

## Platforms

| Platform | Status                                         |
| -------- | ---------------------------------------------- |
| Linux    | Primary target, with Xorg                      |
| BSD      | Should work (X11 only), not tested             |
| Wayland  | Not planned                                    |

## Building

Requirements: a C++17 compiler, GNU make, `pkg-config` and the development files
of `x11`, `xft`, `xrender` and `xext`, and `xrandr` and `xinerama` for several
monitors (they are used when they are installed; `make RANDR=0 XINERAMA=0`
builds without them and the whole screen is one monitor).

```sh
sudo apt install build-essential pkg-config \
    libx11-dev libxft-dev libxrender-dev libxext-dev libxrandr-dev \
    libxinerama-dev libfontconfig-dev

make                 # build/tiny-shell, tiny-settings, tiny-fonts, tiny-about, tiny-wallpaper
make release         # the same, small and stripped, into build-release/
make DEBUG=1         # into build-debug/, with the debugging information (-Og -g), much bigger,
                     # and with diagnostic messages on stderr (TINY_LOG)
```

### Installation

```sh
sudo make install                 # into /usr/local
make install PREFIX=/usr DESTDIR=/tmp/stage   # a package: /tmp/stage/usr/...
sudo make uninstall               # the same PREFIX
```

It installs the programs into `bin/` and the contents of `data/` into `share/`:

- `share/tiny-shell/icons/white` and `black`: the symbolic icons of the shell.
  The shell paints them with the color of the text, so it reads the `white`
  ones; the `black` ones are for other programs. The Icon Naming Specification
  names are used (`start-here` is the logo, `window-close`, `view-app-grid`,
  `application-x-executable` is the icon of an application that has none), and
  four names are the shell's own: `window`, `window-shade`, `window-switch` and
  `workspace-switcher`. Without the files the shell draws the symbols as before.
  `tools/icons/gen_icons.py` makes the set.
- `share/icons/hicolor/scalable/apps`: `tiny-shell`, `tiny-settings` and
  `tiny-fonts`, the icons of the windows of the programs.
- `share/applications`: the desktop files of `tiny-settings`, `tiny-fonts` and
  `tiny-about`, so that the launcher lists the first two and the dock finds the
  icons of their windows (`StartupWMClass`).
- `share/backgrounds/tiny-shell`: the wallpapers (a landscape, a landscape with
  a fox, dusk and night, each in 1920x1080 and 3840x2160).

The files are searched in `$XDG_DATA_HOME`, `$XDG_DATA_DIRS` and
`/usr/local/share`. To try the shell from the build directory with the data from
the repository, run it with `XDG_DATA_DIRS=$PWD/data:/usr/share` and `build` in
the `PATH` (the script `tools/run-xephyr.sh` does it).

To run the tests (they need `xvfb` and googletest):

```sh
sudo apt install xvfb libgtest-dev
make check
make coverage        # coverage report
```

## Trying it

The safest way is a nested X server. The script builds the window manager and
starts it in `Xephyr` with a session from `local/xinitrc`, which it creates from
[examples/xinitrc](examples/xinitrc) on the first run (`local/` is not in git,
so you can change it freely; see the script for `local/env.sh`):

```sh
tools/run-xephyr.sh
```

To use tiny-shell as your real window manager, run `exec build/tiny-shell` at
the end of your `~/.xinitrc`. Set `scale` (for example `1.5` or `2`) in the
configuration file to scale the theme on a high-DPI screen.

### Configuration

The settings are in an ini file, `tiny-shell.ini`, read from two places and
layered: the global one in `$XDG_CONFIG_DIRS` (`/etc/xdg` by default; set the
variable to `/usr/local/etc/xdg` where the system keeps its files there, e.g. on
the BSDs) and then the one of the user in `$XDG_CONFIG_HOME` (`~/.config` by
default). A key of the user's file replaces the same key of the global file, so
it only needs the keys that differ. Both files are optional, `tiny-shell --info`
lists them and the settings in use. The file is read when tiny-shell starts and
again when it gets `SIGHUP` or is run as `tiny-shell --reload` (see below); a
line or a value that is wrong is reported on stderr and the default is used.

```ini
# A comment is a whole line that starts with # or ; (not the end of a line)
[theme]
# A number above 0, up to 16
scale = 1.5
# The font of the shell: a family (see tiny-fonts for the names) or a list of
# them, separated by commas; the next ones are used for the characters that the
# first one does not have, and when it is not installed
font = DejaVu Sans,sans
# The size in points at the scale 1, from 4 to 96
font_size = 12
# The colors: dark (the default) or light
style = dark
# The style of the font, a name that the family has ("Bold", "Italic"); empty
# is the default style. The title bars are always bold.
font_style = Bold
# Tell GTK and Qt programs the theme, the font and the DPI (XSETTINGS, see
# below): yes (the default) or no
xsettings = yes

[xft]
# How the programs draw text, put into the X resources Xft.* when tiny-shell
# starts. Empty (or no key) means "not set": the system decides.
# yes or no
antialias = yes
# none, slight, medium or full
hinting = slight
# The order of the subpixels of the display: none, rgb, bgr, vrgb or vbgr
rgba = rgb

[wallpaper]
# A PNG or JPEG file; empty is no wallpaper, no key is the wallpaper of the
# style (see Wallpaper below). ~/ is the home.
file = ~/Pictures/mountains.jpg
# fill (the default), center or tile
mode = fill

[commands]
# Restart and Shutdown in the main menu (these are the defaults)
reboot = systemctl reboot
poweroff = systemctl poweroff
# Runs the applications with Terminal=true, the command follows it; the
# Terminal item of the main menu starts the program alone (xterm)
terminal = xterm -e

[panel]
# The clock at the right end of the panel: a format of strftime(); empty is
# no clock. The names of the days and the order of the date follow LC_TIME / LANG.
# It changes at the start of every minute (the default is below)
clock_format = %a %e %B %R

# The icons of programs (the system tray) in the panel, left of the clock:
# yes (the default) or no. Set no when another tray (stalonetray, trayer) runs.
system_tray = yes

[monitors]
# The monitors are numbered from the left (and from the top when they start at
# the same place). The monitor of the panel, the dock, the launcher and the
# dialogs: auto (the primary monitor of RandR), a number, or the name of an
# output (DP-1)
primary = auto
# The monitor that placement moves: auto (the first one that is not the
# primary), a number or the name of an output
secondary = auto
# Where the secondary monitor is, seen from the primary one: keep (as the
# system has it), right, left, above or below. It runs xrandr.
placement = keep

[dock]
# When the dock is shown: autohide (the default: it is hidden while the active
# window covers it, and the pointer at the bottom edge of the screen shows it
# until the pointer leaves the dock), on (always) or off (never)
mode = autohide
# Applications that stay in the dock when they do not run: the ids of their
# desktop files, separated by ;. The menu of the dock writes it (see below)
favorites = firefox.desktop;org.gnome.Nautilus.desktop
```

[examples/tiny-shell.ini](examples/tiny-shell.ini) is a file to start with. Use
`sudo reboot`, `doas poweroff` or similar where there is no systemd.

tiny-shell writes the file of the user (never the global one) when you change
the favorites of the dock, and so does `tiny-settings` (see below). It makes the
whole file again from the values that were in it, so **your comments are lost**
and the sections come in the order of their names; the values of the global file
are not copied into it. The new file replaces the old one in one step, and a
failure is reported on stderr and leaves the old file as it was. The file says
so at the top.

### The clock

The date and the time are at the right end of the panel. The text is made by
`strftime()` from `[panel] clock_format`, so the names of the days and months
and the order of the date follow the language of the session (`LC_TIME`, set by
`LANG` or `LC_ALL`): `%e` is the day, `%B` the name of the month (`čt 5
listopadu 15:33` in Czech, `Thu 5 November 15:33` in English) and `%R` is
`HH:MM`. The default is `%a %e %B %R`, without the year. The clock is as wide as
the widest text of the format in a year, so the panel does not move, and it
changes at the start of every minute, so it does not show seconds. An empty
format removes the clock.

### Settings for GTK and Qt (XSETTINGS)

tiny-shell is the manager of settings of the screen, the way the
[XSETTINGS](https://specifications.freedesktop.org/xsettings-spec/latest/)
specification describes it: it owns the selection `_XSETTINGS_S0`, tells the
programs with the `MANAGER` message and keeps the settings in the property
`_XSETTINGS_SETTINGS` of its window. The programs that follow it (GTK 2 and 3,
Qt with the GTK platform theme, Firefox...) change at once when a setting
changes. The settings come from the configuration file:

- `Net/ThemeName`: `Adwaita-dark` for `[theme] style = dark`, `Adwaita` for
  light (GTK 2 and 3 can choose a dark theme only by its name; GTK 4 and
  libadwaita read the color scheme of the portal instead, not done yet)
- `Gtk/FontName`: the first family of `[theme] font`, `font_style` and
  `font_size`, like "DejaVu Sans Bold 12"
- `Xft/DPI`, `Gdk/UnscaledDPI` and `Gdk/WindowScalingFactor`: 96 dpi times
  `[theme] scale`; the whole part of the scale is the factor of the windows
- `Xft/Antialias`, `Xft/Hinting`, `Xft/HintStyle` and `Xft/RGBA` from `[xft]`
  (what is not set is not published)

A settings daemon that already runs (`gsd-xsettings`, `xsettingsd`) is left
alone and tiny-shell publishes nothing; the same happens with `[theme] xsettings
= no`. The cursor theme is not published yet.

### The system tray

The icons of programs (a network applet, a volume control, a messenger) are in
the panel, left of the clock, the newest one at the left end. tiny-shell is the
tray when it starts, the way the [System Tray
Protocol](https://specifications.freedesktop.org/systemtray-spec/latest/) and
[XEMBED](https://specifications.freedesktop.org/xembed-spec/latest/) say: it
owns the selection `_NET_SYSTEM_TRAY_S0`, tells the programs with the `MANAGER`
message, and a program asks for an icon with `_NET_SYSTEM_TRAY_OPCODE`. An icon
that the program hides (`_XEMBED_INFO`) takes no place, and one that the program
destroys is gone from the panel. The texts of balloons (`BEGIN_MESSAGE`) are not
shown.

`[panel] system_tray = no` turns the tray off. Another tray that already runs is
left alone (there is a message and no tray in the panel), and the tray stops
when another one takes the selection; the icons go back to their programs, which
ask again when a tray announces itself.

`make examples` builds `build/examples/tray_icon`, a program with one icon in
the tray and a menu with Quit. It draws one of the icons of the shell, so start
it with the data of the repository (`tools/run-xephyr.sh` already has it in
`XDG_DATA_DIRS`):

```sh
XDG_DATA_DIRS=$PWD/data:/usr/share build/examples/tray_icon
```

### Show desktop

The action `show-desktop` (or a request of a program or a pager,
`_NET_SHOWING_DESKTOP`) minimizes all the windows to show the desktop, and
asking again brings back the windows it hid. The windows that were minimized
before stay so. Focusing any window (Alt+Tab, the dock, a new window) ends the
mode and brings the others back. There is no key and no button for it yet.

### The window menu

The menu has what a window can do and the title bar has no button for. It opens:

- with the button at the left end of the title bar (below it), or with the
  right button anywhere on the title bar (at the pointer);
- with **Alt+Space** for the active window (below its title bar), or the action
  `window-menu`;
- when a program that draws its own title bar (GTK) asks for it with
  `_GTK_SHOW_WINDOW_MENU`, at the place it gives. The shell announces the
  request in `_NET_SUPPORTED`.

The items depend on the window, and what the window cannot do is not listed:

- **Maximize** or **Restore** (a maximized window has no Move, Resize and
  Roll Up);
- **Minimize**;
- **Move** and **Resize**: the window follows the pointer until you click (any
  of the three buttons), Escape puts it back. Moving does not need the pointer
  to go anywhere first, the window keeps its distance from it. Resizing takes
  the bottom right corner of the window to the pointer, so move the pointer
  to the corner you want;
- **Roll Up** or **Roll Down**: the window is rolled up to its title bar (a
  double click on the title bar does the same);
- **Always on Top** or **Not Always on Top**;
- **Close**, last.

A window without the `MENU` decoration of `_MOTIF_WM_HINTS` and a dialog have
no menu button (the right button and Alt+Space still open the menu).

### Startup notification

An application that tells when it has started (`StartupNotify=true` in its
desktop file) is started by the launcher or the dock with `DESKTOP_STARTUP_ID`
in its environment, and the pointer over the desktop is the "busy" one
(`XC_watch`) until its first window comes. The window ends the wait with
`_NET_STARTUP_ID`, or with the `WM_CLASS` that `StartupWMClass` names. The wait
also ends when the launch fails, and after 15 seconds, so a program that does
not know the protocol cannot keep the cursor busy. Other launchers can use the
[startup notification
protocol](https://specifications.freedesktop.org/startup-notification-spec/latest/)
too: tiny-shell watches the messages `_NET_STARTUP_INFO_BEGIN` and
`_NET_STARTUP_INFO` that they send to the root window. The dock does not mark
the starting application yet.

### Settings

`build/tiny-settings` (made by `make`) has a panel at the left with the sections
and the fields of the chosen section:

- **Fonts:** the font (a family or a list of families separated by commas, like
  `DejaVu Sans,sans`), its size and style, and the rendering of text.
- **Appearance:** the scale, the wallpaper and how it covers the screen (a small
  picture of the file above the path shows that the path is right), the mode of
  the dock (Auto-hide, On, Off) and the format of the clock.
- **Applications:** the terminal.
- **Screens:** the primary and the secondary monitor (a choice of the monitors
  that the system has, like `1: DP-1 1920x1080`, or Auto), where the secondary
  one is (Keep, Right, Left, Above or Below the primary one). While this section
  is shown, the number of every monitor is on its screen, over all the windows,
  and Save applies the monitors at once.

A click on a section, or Ctrl+Page Down and Ctrl+Page Up, shows it. The form
shows the values in use now (the global file and the one of the user together).
A field has a cursor: the arrows, Home, End, Backspace and Delete work, and a
click puts the cursor where the pointer is. Ctrl+V, Shift+Insert and the middle
button paste the text of the clipboard or of the selection (one line of it),
Ctrl+C copies the text of the field. Tab moves between the fields of the
section. A field with a wrong value turns red and says what is wrong, and Save
waits until it is right (and shows the section where the wrong field is).

The button Choose... (or Down in the font field) shows a list of the fonts of
the system: type to search it, move with the arrows, Page Up and Page Down.
Under the list are the size and the style of the font, and a preview of a
sentence in the selected family, size and style, so you see how it will look
(the size follows `[theme] scale`, like in the shell). Tab moves between the
search, the size and the style; Up and Down change the size by a point in the
size field, and the arrows or a click change the style in the style field (it
offers the styles that the family has). Select (or Return) takes the three into
the form, Cancel (or Escape) goes back and changes nothing.

The rows Antialias, Hinting and Subpixel are choices, not text: the left and
right arrows, Space and a click go through the values, Backspace goes back to
Default. Default means that nothing is set and the programs keep what the system
(fontconfig) says. They set how the programs draw text, the way a GNOME settings
daemon does it for GTK, but through the X resources `Xft.antialias`,
`Xft.hinting`, `Xft.hintstyle` and `Xft.rgba`, which Xft (and so tiny-shell and
the programs that use Xft or read the resources, like Qt on X11) reads when a
program draws its first text. Save puts them into the `RESOURCE_MANAGER`
property of the root window at once (the other resources there stay), so the
programs that you start later use them; a program that runs keeps its look until
it is started again. tiny-shell also puts the values of the file there when it
starts, before it opens its first font. GTK programs read their own channels
(XSETTINGS, GSettings), which are not set by this.

The row Wallpaper is the path of a PNG or JPEG file, typed or pasted (there is
no file browser: it would be a program of its own, and Xlib has none). The row
Placement is a choice like the three above: Fill, Center or Tile. When one of
them changed, Save starts `tiny-wallpaper` at once, so you see the picture
without a new login. To remove a wallpaper, clear the row: the plain color of
the theme is back after the next login.

Save (or Return) writes the values you changed (`[theme] font`, `font_size`,
`font_style`, `[theme] scale`, `[commands] terminal`, `[xft] antialias`,
`hinting`, `rgba`, `[wallpaper] file`, `mode`, `[monitors]` ...) to the file of
the user, so the others still come from the global file, in the way that is
described above (the comments of the file are lost), and closes the program.
Cancel or Escape closes it and writes nothing. tiny-shell reads the file only
when it starts, so a change of the rest is seen after you log out and in again.

### Wallpaper

`build/tiny-wallpaper [-m fill|center|tile] [-s dark|light] [FILE]` draws a
picture as the background of the root window. Without arguments it uses
`[wallpaper] file` and `mode` of the configuration, so `tiny-shell` starts it
that way when a file is set, and again each time the size of the screen changes
(RandR, a new monitor): the window manager only watches the size of the root
window, it does not link the decoders. When the configuration has no
`[wallpaper] file` (an empty one means no wallpaper), the wallpaper is the one
that `make install` puts in `share/backgrounds/tiny-shell`: the landscape with
the fox (`tiny-shell-landscape-fox-1920x1080.png`) for the light style and the
night (`tiny-shell-night-1920x1080.png`) for the dark one. It changes with the
style (also with the actions `theme-dark`, `theme-light` and `theme-toggle`: the
shell starts the program again with `-s`); a wallpaper that is set in the file
does not. The modes:

- **fill** (the default): the picture is scaled to cover the screen and the part
  that sticks out is cut, from all the sides equally;
- **center**: not scaled, in the middle of the screen, black around it (or cut
  when it is bigger);
- **tile**: not scaled, repeated from the top left corner.

The formats are PNG and JPEG (baseline and progressive; the kind of the file is
found by its content, not by the extension). A transparent part is shown black.
A picture of more than 25 million pixels (a 5K screen has 14.7) or a file over
64 MB is refused with a message on stderr. The picture is decoded, drawn and
freed: the program does not stay in memory. The pixmap stays in the X server as
`_XROOTPMAP_ID` and `ESETROOT_PMAP_ID`, as `Esetroot` leaves it, and the next
run frees the old one. A screen of several monitors is one picture over all of
them. The picture does not change with the light and dark variants of the theme
when it is set in the file (the plain color does). The path must be absolute (or
start with `~/`), because the program starts in another directory. tiny-shell
watches the size of the screen only when a wallpaper is set in the file when it
starts, so the first wallpaper that you set in `tiny-settings` is drawn at once,
but it follows a change of the screen after the next login. A file that cannot
be read is reported on stderr and the plain color stays.

The decoder of photos is a second copy of `stb_image` in
`lib/third_party/vendor_photo.cc`, with only PNG and JPEG on: it is linked into
`tiny-wallpaper` only, so `tiny-shell` does not carry the JPEG code.

### The main menu

The logo at the left of the panel opens a menu with these items, in groups
(without the installed icons the button is the text Main Menu):

- **About Tiny Shell** starts `tiny-about`, a window with the logo, the version,
  the name of the system (`PRETTY_NAME` of `/etc/os-release`, or the name of
  `uname`), the kernel with the kind of the processor, the license and the
  address of the project. It is a window of its own, so the dock shows the logo
  for it. Escape, Return, Ctrl+Q and the button Close end it.
- **Settings** starts `tiny-settings` (the one next to `tiny-shell` when it was
  started with a path, else the one in `PATH`).
- **Applications** opens the launcher; **Terminal** starts the terminal of
  `[commands] terminal` (without the `-e` at its end).
- **Force Quit** starts `xkill`: click a window to end its program.
- **Restart** and **Shutdown** run `[commands] reboot` and `poweroff`.
- **Logout** ends tiny-shell.

A click on an item closes the menu. A program that cannot be started is reported
on stderr.

### Multiple monitors

The monitors are the ones of RandR (`XRRGetMonitors`, also the ones that `xrandr
--setmonitor` makes, e.g. to split a wide monitor). When RandR knows only one,
the screens of Xinerama are used (a nested Xephyr started with `+xinerama` has
them only there; such monitors have no names, so the position cannot be
changed). They are numbered from the left to the right, and from the top to the
bottom when two start at the same place; a monitor that shows the same picture
as another (a clone) is not counted. Without RandR the whole screen is one
monitor.

- **The shell is on the primary monitor.** `[monitors] primary` chooses it by
  its number or the name of its output; `auto` is the primary monitor of RandR
  (`xrandr --primary`), or the first one. The panel, the dock (and the strip
  that shows it), the launcher, the run dialog and the window switcher are
  there. A dock on every monitor is not done (the dock is one widget with one
  list of windows).
- **Windows.** A window opens where the pointer is when it has no place of its
  own, is maximized to the work area of the monitor it is on (the monitor that
  has most of the frame), and fullscreen covers that monitor only. The panel
  takes the top of the primary monitor, the struts of other docks reduce the
  monitors they touch: a strut is measured from the edge of the whole screen (as
  `_NET_WM_STRUT` says), so a top strut does not touch a monitor that is below
  another one. `_NET_WORKAREA` is the work area of the primary monitor.
- **Position.** `[monitors] placement` puts the secondary monitor to the
  `right`, `left`, `above` or `below` the primary one by running `xrandr
  --output SECONDARY --right-of PRIMARY` (the program `xrandr` must be
  installed). With `keep` (the default) the layout of the system is left as it
  is. The command is run again when the monitors change, only when the placement
  is not as asked, and not twice in a row. `secondary` must be the name of an
  output for it to work (the monitors that `--setmonitor` made are not outputs).
- **Numbers.** Only `tiny-settings` shows them: while the section Screens is
  shown, the number, the name and the size of every monitor are in its corner,
  over all the windows (the primary one says so), and they go away when the
  section is left or the settings are closed (the settings ask the shell for it
  with the client message `_TINY_SHELL_MONITORS`).

Save in `tiny-settings` makes the shell read `[monitors]` again at once (the
same message), so the primary monitor and the position are changed without a new
login; the other settings still need one. A monitor that is plugged in or out,
or moved by `xrandr`, is used at once. The position needs a server with more
than one output: a nested X server like Xephyr has a single one, so there is
nothing to move there (try it on a real display, or `xrandr --listmonitors`
shows what the shell sees).

### The dock and the favorite applications

`[dock] mode` (or the setting Dock in `tiny-settings`) says when the dock is
shown. **autohide** (the default) hides it while the active window covers any
part of it, and brings it back when the pointer reaches the bottom edge of the
screen (a strip of a few pixels, scaled with `[theme] scale`); it hides again
when the pointer leaves it, but not while a menu of the dock is open or an icon
is dragged. **on** always shows it, **off** never does.

The dock has one icon for every application. All the windows of an application
(it is found by `WM_CLASS`, see below; windows of an unknown application are
grouped by their class) share the icon, and a small dot under it stands for
every window; the dot of the window that has the focus is brighter. The favorite
applications come first, a thin line separates them from the other running
applications, and they stay in the dock when they do not run.

Click the icon of a favorite that does not run to start it. Click the icon of an
application that runs to focus its window; when it has more windows, every next
click goes to the next one. Drag the icon of a favorite to move it: a mark shows
where it will be put, the icon is drawn smaller meanwhile, and the new order is
saved; Escape cancels the drag and nothing moves.

Click the right button on an icon to open its menu:

- **New Window** starts the application again, also when it runs.
- The other actions of the desktop file (`[Desktop Action ...]`, for example
  "New Private Window").
- **All Windows ›** (when it runs) opens a submenu with the titles of its
  windows; a click focuses the window.
- **Add to Favorites** or **Remove from Favorites**.
- **Close Window**, or **Close All Windows** when there are more (when it runs),
  as if you clicked the close button of the windows.

The menu is as wide as its longest text. A click anywhere outside of the menu
closes it (the click does not reach the window below). In the launcher the right
button on an application opens the same menu, so you can put an application in
the dock without starting it. The favorites are saved at once to `[dock]
favorites`. An id of an application that is not installed is kept in the file
but not shown.

### Fonts

`build/tiny-fonts` (made by `make`) shows the font families of the system, the
list that `fc-list : family` prints, read from fontconfig. Every family is a
cell with "Aa" in the family (black on white) and its name under it. Type at the
top to search the families (Escape clears the search). Scroll with the wheel or
the keys, click a family (or press Return) to see its specimen: the small
letters, the capitals, the digits and the signs, then one sentence in 8, 12, 16,
20, 24, 32, 64 and 128 pixels. The button Info (or I) at the top right shows
what is known about the family: the styles, the format, the languages, the files
of its faces and, from the font file itself, the full name, the version, the
designer, the license and the copyright. The header has the breadcrumbs "‹ Fonts
‹ Family": a click on Fonts goes to the list, a click on the family back to the
specimen. Escape and backspace go one step back, Ctrl+Q and Ctrl+W quit. The
sizes follow `[theme] scale`.

### Command line

```
tiny-shell [--replace] [--info] [--reload] [--version] [--help]
```

`--replace` takes over from a window manager that is already running, the way
ICCCM describes it (the `WM_S0` selection): the old one is asked to leave and
tiny-shell starts when it has gone. Another window manager started with its own
`--replace` takes over from tiny-shell in the same way. Window managers that do
not use the selection (for example twm) have to be stopped first. `--info`
prints what tiny-shell is and which settings it uses. `--reload` tells the
tiny-shell that runs on the display to read the configuration files again; it is
the same as `kill -HUP` of its process (the process is found through
`_NET_WM_PID` of the window `_NET_SUPPORTING_WM_CHECK`, so it works on the
computer where tiny-shell runs). What changes at once: the colors (`[theme]
style`), the favorites, the commands, the wallpaper, the clock, the system tray,
the mode of the dock, `[xft]` and what the programs are told by XSETTINGS. The
scale and the font of the shell itself need a restart (a message says so).
`SIGTERM` and `SIGINT` end tiny-shell.

### Where the launcher looks for applications

The launcher reads the `.desktop` files from the `applications` directory of
each of these, the first one wins when an application is in more of them:

1. `$XDG_DATA_HOME` (`~/.local/share` when it is not set)
2. the directories of `$XDG_DATA_DIRS`, in their order
   (`/usr/local/share:/usr/share` when it is not set)
3. `/usr/local/share`, if the list above does not have it

The last step is the only thing added on top of the XDG specification, the same
way as GNOME does it; you can move `/usr/local/share` to any place by listing it
in `XDG_DATA_DIRS`. Other prefixes (for example `/usr/pkg/share` on NetBSD)
belong to the session, so set them in `~/.xinitrc`.

The launcher starts a program with the `PATH` of tiny-shell, so put
`~/.local/bin` and the like to `PATH` in the session too.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md), which lists the core decisions
of the project.

## License

tiny-shell is released under the [BSD 2-Clause License](LICENSE).
