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
> minimizes, maximizes and closes windows, and has a panel, a dock and a run
> dialog. Large parts of EWMH, the task bar with a clock, `.desktop` files
> and the launcher are still missing, see below.

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

Done:

- [x] Reparenting frames with a title, icon and close, minimize and maximize
      buttons
- [x] Buttons and resizing follow the window: `_MOTIF_WM_HINTS` functions,
      min and max size, windows without decorations
- [x] Move and resize by the title bar, edges and corners; snapping to the
      resize increment of the client
- [x] Focus, `WM_TAKE_FOCUS`, `WM_DELETE_WINDOW` (with `XKillClient` as the
      fallback)
- [x] Window titles from `_NET_WM_NAME` and `WM_NAME`, icons from
      `_NET_WM_ICON` and `WM_HINTS`
- [x] Minimize, maximize and restore, with `_NET_WM_STATE` kept up to date
- [x] Existing windows are adopted when the manager starts
- [x] `DESKTOP`, `DOCK` and `SPLASH` windows are left unmanaged
- [x] Top panel with an application menu and a user menu (logout)
- [x] Dock with the icons of the running windows
- [x] Run dialog (Alt+F2) and window switching (Alt+Tab, Alt+Shift+Tab)
- [x] Scaling of the whole theme for high-DPI screens
      (`TINY_SCALE_FACTOR`)
- [x] Unit tests of the toolkit and of the window manager against Xvfb

Planned, grouped by importance:

### Must: a window manager other programs can rely on

- [ ] EWMH root properties (`_NET_SUPPORTED`, `_NET_SUPPORTING_WM_CHECK`,
      `_NET_CLIENT_LIST`, `_NET_ACTIVE_WINDOW`, `_NET_WORKAREA`) and the
      client messages for state, activation and closing
- [ ] Correct window life cycle (destroyed windows, minimized windows, unmap
      caused by the manager)
- [ ] Correct `ConfigureRequest` handling for framed windows, with the
      synthetic `ConfigureNotify`
- [ ] Panel struts and the work area: maximized windows do not cover the
      panel
- [ ] `WM_TRANSIENT_FOR`, modal dialogs and fullscreen
- [ ] An event loop that can wait for timers and child processes as well as
      for X events, without zombie processes

### Should: what people expect from a desktop

- [ ] Clock (date and time) in the panel
- [ ] Task bar in the panel (the "old style" task list), as an alternative to
      the dock
- [ ] Desktop Entry files (`.desktop`), application menu grouped by category
- [ ] Launcher with a list of all applications and search
- [ ] Icon theme lookup (hicolor and pixmaps) and PNG icons
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

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md), which lists the core
decisions of the project.

## License

tiny-shell is released under the [BSD 2-Clause License](LICENSE).
