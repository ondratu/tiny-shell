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
> dialog and an application launcher. A clock, a task bar
> and a configuration file are still missing, see below.

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
  (`TINY_SCALE_FACTOR`) and configured within reason.

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
  work area, active window, focus stealing prevention, frame extents, the
  client lists), `_MOTIF_WM_HINTS` (functions and decorations), Desktop Entry
  and the Icon Theme specification (PNG, SVG, XPM).
- **Shell:** top panel with an application menu and a user menu, a dock with
  the icons of running windows, a run dialog (Alt+F2), a full screen launcher
  with search (Super).
- **Other:** one `poll()` event loop for X events, signals and timers
  (finished children are collected by a `SIGCHLD` handler), commands of Restart and
  Shutdown (`--restart-command`, `--shutdown-command`), taking over from
  another window manager (`--replace`) and handing over to one, Super + mouse moves a window, scaling of the whole theme for
  high-DPI screens (`TINY_SCALE_FACTOR`), unit tests of the toolkit and the
  window manager against Xvfb.

Planned, grouped by importance:

### Must: a window manager other programs can rely on

- [ ] Geometry details: the work area follows a dock that appears later, a
      window that starts maximized keeps its aspect ratio

### Should: what people expect from a desktop

- [ ] Clock (date and time) in the panel
- [ ] Task bar in the panel (the "old style" task list), as an alternative to
      the dock
- [ ] Application menu grouped by category
- [ ] Most recently used order for Alt+Tab
- [ ] Configuration file: key bindings, panel layout, fonts
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
  Xft and Xrender; nothing else is needed to run it
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
    libx11-dev libxft-dev libxrender-dev

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
end of your `~/.xinitrc`. Set `TINY_SCALE_FACTOR` (for example `1.5` or `2`)
to scale the theme on a high-DPI screen.

### Command line

```
tiny-shell [--replace] [--restart-command=CMD] [--shutdown-command=CMD]
           [--info] [--version] [--help]
```

`--replace` takes over from a window manager that is already running, the way
ICCCM describes it (the `WM_S0` selection): the old one is asked to leave and
tiny-shell starts when it has gone. Another window manager started with its
own `--replace` takes over from tiny-shell in the same way. Window managers
that do not use the selection (for example twm) have to be stopped first.
`--info` prints what tiny-shell is and which settings it uses.
`--restart-command` and `--shutdown-command` set the commands of Restart and
Shutdown in the user menu (`systemctl reboot` and `systemctl poweroff` by
default; use `sudo reboot`, `doas poweroff` or similar where there is no
systemd).

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
