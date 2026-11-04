# tiny-shell

A very small and light window manager and desktop shell for X11, written
directly on top of Xlib.

*tiny-shell* is meant to be a tiny alternative to GNOME Shell: a reparenting
window manager with a panel, a dock, a menu and a launcher, in a few thousand
lines of C++ that depend on nothing but libX11 and a couple of light X
libraries. It targets embedded Linux systems that run Xorg, and it is also a
proof of concept: a way to learn Xlib properly and to squeeze out of it as
much as it can give.

> **Status:** a working prototype. It manages, decorates, moves, resizes,
> minimizes, maximizes and closes windows, and has a panel, a dock, a run
> dialog and an application launcher. A clock and a task bar are still
> missing, see below.

## What it is

- A reparenting window manager with title bars, buttons and borders drawn by
  tiny-shell itself.
- A small shell around it: top panel, application and user menus, a dock
  with the icons of running windows, a run dialog.
- A set of freedesktop.org standards implemented as faithfully as possible
  (ICCCM, EWMH, Desktop Entry, Icon Theme), so that ordinary applications
  just work.
- A tiny widget toolkit (`lib/`), made of plain X windows and Xft text,
  loosely inspired by GTK. It exists to serve the shell, not to be reused.

## What it is not

- Not a desktop environment. No file manager, settings application, session
  manager or desktop icons (use an external tool such as idesk if you need
  them).
- Not a compositor. No transparency, shadows or animations.
- Not a Wayland compositor and not planned to be one. X11 on Xorg only.
- Not a replacement for a full GNOME or KDE session on a desktop computer.
- Not themable to the last pixel. There is one look, which can be scaled
  (`[theme] scale` in the configuration file) and configured within reason.

## Features

Implemented:

- **Window manager:** reparenting frames drawn by tiny-shell (title, icon,
  close, minimize and maximize buttons), move and resize by the title bar,
  edges and corners, maximize, fullscreen (F11), minimize (also on request of
  the client), shade, dialogs and modal dialogs, focus handling, Alt+Tab,
  windows adopted at start, clients survive a crash of the manager and are
  given back when it exits, applications that stop responding are found by
  `_NET_WM_PING` and the user chooses to wait or to force them to quit.
- **Standards:** ICCCM, EWMH (window states, types, layers, struts and the
  work area that follows docks coming and going, active window, focus
  stealing prevention, frame extents, the client lists), `_MOTIF_WM_HINTS`
  (functions and decorations), Desktop Entry and the Icon Theme specification
  (PNG, SVG, XPM).
- **Shell:** top panel with an application menu and a user menu, a dock with
  one icon for every application (a dot under it for every window) and the
  favorite applications, which stay there when they do not run and can be moved
  by the mouse, a run dialog (Alt+F2), a full screen launcher with search
  (Super).
- **Fonts:** `tiny-fonts`, a small program that lists the fonts of the system
  and shows a specimen of each one (see below).
- **Other:** one `poll()` event loop for X events, signals and timers
  (finished children are collected by a `SIGCHLD` handler), commands of
  Restart and Shutdown, a configuration file (scale, commands, favorites of
  the dock), taking
  over from another window manager (`--replace`) and handing over to one,
  Super + mouse moves a window, scaling of the whole theme for high-DPI
  screens, unit tests of the toolkit and the window
  manager against Xvfb.

Planned, grouped by importance (what a window manager must do for other
programs is done, so there is no "must" list):

### Should: what people expect from a desktop

- [ ] Clock (date and time) in the panel
- [ ] Task bar in the panel (the "old style" task list), as an alternative to
      the dock
- [ ] Application menu grouped by category
- [ ] Most recently used order for Alt+Tab
- [ ] More in the configuration file: key bindings, panel layout, fonts,
      the theme (light or dark), plugins
- [ ] No flicker: double buffered drawing of the panel and the dock

### Could: nice to have

- [ ] Virtual desktops
- [ ] System tray (`_NET_SYSTEM_TRAY`, XEMBED)
- [ ] Multiple monitors (RandR)
- [ ] Plugins and scripting in Lua, as an optional build feature
- [ ] Window snapping to the screen edges
- [ ] Session management

## Technology

- Written in C++17, built with GNU make
- [Xlib](https://www.x.org/releases/current/doc/libX11/libX11/libX11.html),
  Xft (with fontconfig) and Xrender; nothing else is needed to run it
- Tests use [googletest](https://github.com/google/googletest) and Xvfb

## Platforms

| Platform | Status                                         |
| -------- | ---------------------------------------------- |
| Linux    | Primary target, with Xorg                      |
| BSD      | Should work (X11 only), not tested             |
| Wayland  | Not planned                                    |

## Building

Requirements: a C++17 compiler, GNU make, `pkg-config` and the development
files of `x11`, `xft` and `xrender`.

```sh
sudo apt install build-essential pkg-config \
    libx11-dev libxft-dev libxrender-dev libfontconfig-dev

make
```

To run the tests (they need `xvfb` and googletest):

```sh
sudo apt install xvfb libgtest-dev
make check
make coverage        # coverage report
```

## Trying it

The safest way is a nested X server. The script builds the window manager and
starts it in `Xephyr` with a session from `local/xinitrc`, which it creates
from [examples/xinitrc](examples/xinitrc) on the first run (`local/` is not
in git, so you can change it freely; see the script for `local/env.sh`):

```sh
tools/run-xephyr.sh
```

To use tiny-shell as your real window manager, run `exec build/tiny-shell` at the
end of your `~/.xinitrc`. Set `scale` (for example `1.5` or `2`) in the
configuration file to scale the theme on a high-DPI screen.

### Configuration

The settings are in an ini file, `tiny-shell.ini`, read from two places
and layered: the global one in `$XDG_CONFIG_DIRS` (`/etc/xdg` by default;
set the variable to `/usr/local/etc/xdg` where the system keeps its files
there, e.g. on the BSDs) and then the one of the user in `$XDG_CONFIG_HOME`
(`~/.config` by default). A key of the user's file replaces the same key of
the global file, so it only needs the keys that differ. Both files are
optional, `tiny-shell --info` lists them and the settings in use. The file
is read when tiny-shell starts; a line or a value that is wrong is reported
on stderr and the default is used.

```ini
# A comment is a whole line that starts with # or ; (not the end of a line)
[theme]
# A number above 0, up to 16
scale = 1.5

[commands]
# Restart and Shutdown in the user menu (these are the defaults)
reboot = systemctl reboot
poweroff = systemctl poweroff
# Runs the applications with Terminal=true; the command follows it
terminal = xterm -e

[dock]
# Applications that stay in the dock when they do not run: the ids of their
# desktop files, separated by ;. The menu of the dock writes it (see below)
favorites = firefox.desktop;org.gnome.Nautilus.desktop
```

[examples/tiny-shell.ini](examples/tiny-shell.ini) is a file to start with.
Use `sudo reboot`, `doas poweroff` or similar where there is no systemd.

tiny-shell writes the file of the user (never the global one) when you change
the favorites of the dock. It makes the whole file again from the values that
were in it, so **your comments are lost** and the sections come in the order
of their names; the values of the global file are not copied into it. The
new file replaces the old one in one step, and a failure is reported on
stderr and leaves the old file as it was. The file says so at the top.

### The dock and the favorite applications

The dock has one icon for every application. All the windows of an application
(it is found by `WM_CLASS`, see below; windows of an unknown application are
grouped by their class) share the icon, and a small dot under it stands for
every window; the dot of the window that has the focus is brighter. The
favorite applications come first, a thin line separates them from the other
running applications, and they stay in the dock when they do not run.

Click the icon of a favorite that does not run to start it. Click the icon of
an application that runs to focus its window; when it has more windows, every
next click goes to the next one. Drag the icon of a favorite to move it: a mark
shows where it will be put, the icon is drawn smaller meanwhile, and the new
order is saved.

Click the right button on an icon to open its menu:

- **New Window** starts the application again, also when it runs.
- The other actions of the desktop file (`[Desktop Action ...]`, for example
  "New Private Window").
- **All Windows ›** (when it runs) opens a submenu with the titles of its
  windows; a click focuses the window.
- **Add to Favorites** or **Remove from Favorites**.
- **Close Window**, or **Close All Windows** when there are more (when it
  runs), as if you clicked the close button of the windows.

The menu is as wide as its longest text. A click anywhere outside of the menu
closes it (the click does not reach the window below). In the launcher the right
button on an application opens the same menu, so you can put an application in
the dock without starting it. The favorites are saved at once to
`[dock] favorites`. An id of an application that is not installed is kept in the
file but not shown.

### Fonts

`build/tiny-fonts` (made by `make`) shows the font families of the system, the
list that `fc-list : family` prints, read from fontconfig. Every family is a
cell with "Aa" in the family (black on white) and its name under it. Scroll
with the wheel or the keys, click a family (or press Return) to see its
specimen: the small letters, the capitals, the digits and the signs, then one
sentence in 8, 12, 16, 20, 24, 32, 64 and 128 pixels. Escape, backspace or the
button at the top return to the list, Ctrl+Q and Ctrl+W quit. The sizes
follow `[theme] scale`.

### Command line

```
tiny-shell [--replace] [--info] [--version] [--help]
```

`--replace` takes over from a window manager that is already running, the way
ICCCM describes it (the `WM_S0` selection): the old one is asked to leave and
tiny-shell starts when it has gone. Another window manager started with its
own `--replace` takes over from tiny-shell in the same way. Window managers
that do not use the selection (for example twm) have to be stopped first.
`--info` prints what tiny-shell is and which settings it uses.

### Where the launcher looks for applications

The launcher reads the `.desktop` files from the `applications` directory of
each of these, the first one wins when an application is in more of them:

1. `$XDG_DATA_HOME` (`~/.local/share` when it is not set)
2. the directories of `$XDG_DATA_DIRS`, in their order
   (`/usr/local/share:/usr/share` when it is not set)
3. `/usr/local/share`, if the list above does not have it

The last step is the only thing added on top of the XDG specification, the
same way as GNOME does it; you can move `/usr/local/share` to any place by
listing it in `XDG_DATA_DIRS`. Other prefixes (for example `/usr/pkg/share` on
NetBSD) belong to the session, so set them in `~/.xinitrc`.

The launcher starts a program with the `PATH` of tiny-shell, so put
`~/.local/bin` and the like to `PATH` in the session too.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md), which lists the core
decisions of the project.

## License

tiny-shell is released under the [BSD 2-Clause License](LICENSE).
