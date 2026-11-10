# Layout of the sources

Everything is built into `build/`. Includes are written from `lib/`, for
example `#include "wm/dock.h"`.

## Two libraries

Two static libraries are built from `lib/`:

| Library | Directories | Namespace | What it is |
| ------- | ----------- | --------- | ---------- |
| `libtiny-widgets.a` | `lib/*.cc`, `lib/image/`, the vendored libraries | `tiny`, `tiny::image` | the widget toolkit; it includes nothing from `wm/`, `xdg/` and `app/` (`make layers` checks it, and the tests of the widgets link only this library) |
| `libtiny-shell.a` | `lib/wm/`, `lib/xdg/`, `lib/app/` | `tiny::wm`, `tiny::xdg`, `tiny::app` | the window manager and the shell, the freedesktop.org files, and the logic of the programs |

## Programs

Every program is one file in `src/`; the logic that can be tested without a
display is in `lib/`.

| Program | Source | Logic |
| ------- | ------ | ----- |
| `tiny-shell`, the window manager itself | `src/main.cc` | `lib/wm/` |
| `tiny-fonts`, the font viewer | `src/fonts.cc` | `lib/app/font_browser.*` |
| `tiny-settings`, the settings form | `src/settings.cc` | `lib/app/settings_form.*` |
| `tiny-about`, the about window | `src/about.cc` | `lib/app/about.*` |
| `tiny-wallpaper`, draws the wallpaper | `src/wallpaper.cc` | `lib/app/wallpaper.*`, the decoder of photos in `lib/image/photo.*` |
| `tiny-lock`, `tiny-lock-auth`, `tiny-lock-pwcheck`, the screen lock | `src/lock.cc`, `src/lock_auth.cc`, `src/lock_pwcheck.cc` | `lib/app/lock_*.*`, `lib/app/pwcheck.*`; see [The screen lock](screen-lock.md) |

`tiny-fonts` and `tiny-settings` are windows of `tiny::XftWindow` (the window,
the pixmap, the colors, the font cache and the loop are there).

## Other directories

| Directory | What is in it |
| --------- | ------------- |
| `data/` | the layout of the `share/` directory (symbolic icons, application icons, desktop files, wallpapers) that `make install` copies to `PREFIX`; the shell finds them in the XDG data directories and draws without them when they are not installed |
| `tests/` | the unit tests, they mirror `lib/` |
| `examples/` | small toolkit programs (`make examples`) and a default session |
| `tools/` | development scripts |
| `doc/` | this documentation and the source of the web site |
| `local/` | files that belong to one machine (a session script, local settings); not committed, created from `examples/` by the scripts |
