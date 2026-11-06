# Contributing to tiny-shell

Thanks for your interest! This document describes the core decisions of the
project. Everyone is expected to follow them; when a change needs to break
one, discuss it first and update this file together with the change.

## Language

Everything in the repository is in English: source code, comments, commit
messages, issues and documentation.

## Goals that decide everything else

1. **Small.** Small source, small binary, small memory footprint. A feature
   that costs a lot of any of them needs a good reason.
2. **Few dependencies.** The shell runs with libX11, Xft, Xrender and Xext, and
   fontconfig and FreeType that Xft needs anyway (and the C and C++ standard
   libraries).
   See [Dependencies](#dependencies).
3. **Standards first.** Use ICCCM, EWMH and freedesktop.org specifications as
   written. Applications and external tools must work without being told
   anything about tiny-shell. Do not invent private protocols when a
   standard one exists.
4. **Works on embedded systems with Xorg.** No D-Bus, systemd, GLib or GPU
   features are required. Everything extra is optional and off by default.
5. **Correct before featureful.** A window manager that behaves properly with
   ordinary applications matters more than a new widget.

Out of scope (see the README): a desktop environment, a compositor, Wayland.

## Technology and conventions

- **Language:** C++17. Use the standard library, but keep templates and
  exceptions simple; the code is read by people learning Xlib. Do not use
  C++ exceptions for control flow in the event loop. There is no RTTI
  (`-fno-rtti`, it was 15 kB of the binary): no `dynamic_cast` and no `typeid`.
- **Build system:** GNU make, one `Makefile`. Dependencies are found with
  `pkg-config`. No autotools or CMake.
- **Code style:** 4 spaces, braces on the same line, `CamelCase` types,
  `snake_case` functions and members, `#pragma once`, one class per header
  where it is practical. Match the surrounding code, including the density of
  comments. Files are `snake_case`, named after their main class in the
  singular (`container.h`, `run_dialog.h`).
- **Namespaces follow the directories of `lib/`:** `tiny` for the widget
  toolkit (`lib/`), `tiny::wm` for the window manager and the shell (`lib/wm/`),
  `tiny::xdg` for the freedesktop.org files, applications and icon themes
  (`lib/xdg/`), `tiny::image` for decoding, scaling and uploading pictures
  (`lib/image/`) and `tiny::app` for the logic of the programs (`lib/app/`).
  Includes are written from `lib/`: `#include "wm/dock.h"`.
- **Layout:** two libraries are built from `lib/`. `libtiny-widgets.a` is the
  widget toolkit (`lib/*.cc`, `lib/image/` and the vendored libraries); it
  includes nothing from `wm/`, `xdg/` and `app/` (`make layers` checks it, and
  the tests of the widgets link only this library). `libtiny-shell.a` is the
  window manager and the shell (`lib/wm/`), the freedesktop.org files
  (`lib/xdg/`) and the logic of the programs (`lib/app/`). `src/main.cc` is the
  window manager itself, `src/fonts.cc` is the font viewer `tiny-fonts`,
  `src/settings.cc` is the settings form `tiny-settings`, `src/about.cc` is
  the about window `tiny-about` (its lines are in `lib/app/about.*`) and
  `src/wallpaper.cc` draws the wallpaper (`tiny-wallpaper`; its logic is in `lib/app/wallpaper.*`
  and the decoder of photos in `lib/image/photo.*`). The first two are windows
  of `tiny::XftWindow` (the window, the pixmap, the colors, the font cache and
  the loop are there) and their logic is in `lib/app/font_browser.*`
  and `lib/app/settings_form.*`. `data/` has the layout of the `share/` directory (symbolic icons,
  application icons, desktop files, wallpapers) that `make install` copies to
  `PREFIX`; the shell finds them in the XDG data directories and draws without
  them when they are not installed. `tests/` are the unit tests (they
  mirror `lib/`), `examples/` are small toolkit programs (`make examples`) and a
  default session, `tools/` are development scripts.
  Everything is built into `build/`. Files that belong to one machine (a
  session script, local settings) are not committed: they live in the ignored
  `local/`, created from `examples/` by the scripts.
- **Geometry is `int`.** Sizes, offsets and coordinates are `int`, like in
  Xlib structures, so that sums and differences do not wrap around. The
  size of a widget cannot be negative: the constructor and `resize()` clamp
  it to zero. Where an X request takes an unsigned value, convert with
  `tiny::x_dim()` (a window size, limited to 1..65535) or `tiny::x_uint()`
  (other values, never negative).
- **Strictness:** the code compiles without warnings with GCC and Clang, with
  a long list of warnings on (see `WARNINGS` in the Makefile), is formatted by
  `clang-format` (`make format`) and checked by `clang-tidy` (`make lint`) and
  `cppcheck` (`make cppcheck`). Fix the cause of a warning; if it really
  cannot be fixed (a macro or a header of Xlib, the fixed signature of a
  callback), isolate it in one small place with a comment
  (`// cppcheck-suppress <id> ; reason`, `// NOLINT(<check>)`). Test coverage stays as high as is sensible and
  never goes down.

## User interface

tiny-shell does not copy the look of GNOME, but its
[Human Interface Guidelines](https://developer.gnome.org/hig/) are the
reference when we are not sure how something should behave. Take from them:

- **Room to breathe.** Controls are not squeezed together. An item of a menu
  is as high as the panel (`theme.wm_popup_item_height`), the text has a margin
  at both sides (`theme.wm_menu_padding`), a popup is as wide as its longest
  text and the margin, not a fixed width. Every size comes from the theme and
  scales with it.
- **Labels.** A command is a verb ("New Window", "Close Window"). Menu items
  and buttons use header capitalization ("Add to Favorites"), an ellipsis
  ("…") is only for an item that asks for more input before it acts. Menu text
  is aligned to the left.
- **Menus are short and ordered.** The most important item is first, the one
  that ends something (Close) is last. Related items are together. Do not nest
  submenus deeper than one level and keep them short. A context menu is shown
  only when it has something to offer.
- **Do not rely on color alone** to tell a state: the focused window has a
  brighter dot and a lighter background, a window that wants attention has a
  bar.
- **Pointer behavior.** Everything that the pointer does must be undoable or
  harmless: a click outside of a popup only closes it, a drag shorter than a
  few pixels is a click, a dragged thing shows where it will land and what it
  leaves behind. Do not use the middle button, double clicks or chords for
  anything essential. A pointer action should also be possible from the
  keyboard (not everything is yet).
- **Quiet defaults.** No animation, no sounds, no popup that the user did not
  ask for.

The shell is a dock, a panel and a launcher, so where GNOME has an idea for
them (the dash of GNOME Shell: running applications with a dot per window,
a line between favorites and the others, drag to reorder), follow it unless it
costs a lot of code or memory (see the goals above).

## Architecture

- **Layers.** The toolkit (`tiny::`) knows nothing about window management.
  The window management core (managing client windows, ICCCM, EWMH) does not
  depend on the panel, the dock or the launcher. The shell (panel, dock,
  menus, launcher) uses the core through a small interface, so that it can
  be replaced or moved to another process later.
- **One event loop.** All work runs in the single `poll()` based loop of the
  manager: X events, timers (clock) and child processes. Never block it:
  no sleeping, no waiting for a process, no slow file reading in a handler.
- **Event handlers.** New code connects events with `std::function`
  (a lambda capturing `this`), registered and unregistered by RAII.
  Do not add new casts between unrelated pointer-to-member types.
- **No global state in new code.** The display, the themes (`tiny::theme`,
  `wm::shell_theme`) and the handler table are global today; new code takes what
  it needs as an argument (like `LauncherLayout::compute()` takes the theme),
  so that it can be tested and the globals can be removed later.
- **Actions have names.** What a menu item, a key or a script can ask for is
  registered by name in the `ActionRegistry` of the manager ("logout",
  "run-dialog", "theme-toggle"); do not connect a new menu item to a private
  handler of the manager. A plugin gets a panel slot from `Panel::add_applet()`.
- **Two looks, same sizes.** The theme has the variants `Dark` (the default)
  and `Light`; a color belongs to `Style::colorize()`, never to a constructor
  or a widget, so that the variant can change while the shell runs
  (`Widget::restyle()`).
- **Quiet by default.** A message for developers ("unhandled property") is
  written with `TINY_LOG()`, which prints only in a `make DEBUG=1` build. A
  problem the user can act on goes to `tiny::error()`.
- **Standards are data.** Atoms, window types and states are listed in one
  place (`display.h`). Do not spread `XInternAtom` calls over the code.
- **Decide without X when you can.** What can be decided without a display
  (which buttons a window has, the order of Alt+Tab, parsing a `.desktop`
  file, a search match, a layout computation) belongs in a plain function
  or class with unit tests. Handlers only connect it to X.
- **Be a good X11 citizen.** Check for errors of requests on windows that may
  be already gone (`BadWindow`), grab the server only for short, bounded
  work, and release grabs. Never leave a client window unreachable when the
  window manager exits: give it back to the root window.

## Dependencies

- Allowed without discussion: libX11, Xft, Xrender, fontconfig and FreeType (Xft
  uses them; we use fontconfig for the list of fonts and FreeType to read the
  `name` table of a font file), libXext (the SYNC extension for resizing
  without tearing) and, when needed for a feature, Xrandr or other extensions
  that are on every Xorg system.
- Everything else needs a discussion first and must be **optional at build
  time** (a make variable, with the feature compiled out when it is off).
  Lua for scripting is the planned example.
- Prefer a small vendored single-file library (public domain or permissive,
  e.g. `stb_image.h`) over a new shared library.
- Vendored libraries live in `lib/third_party/` unchanged, with their licence
  and the version in `lib/third_party/README.md`. They are compiled as system
  headers (`-isystem`), skipped by the formatter, the linter and the
  spell checker, and not counted in the coverage. Update them by copying the
  new files and the version, never by editing.
- Keep binary size and memory use in mind; do not add dependencies only for
  convenience.

## Design principles

- A window is closed with `WM_DELETE_WINDOW` first; `XKillClient` is only a
  fallback.
- The state of a window is what the client and the standards say it is
  (`_NET_WM_STATE`, size hints, Motif hints). The frame follows the client,
  not the opposite.
- Behavior is configured, not themed: a small configuration file for key
  bindings and layout is fine, a theme engine is not.
- One look, scalable for high-DPI (`scale` in the configuration). Every size in code
  comes from the theme, never from a literal number.
- Robustness: a misbehaving client must not crash the window manager.

## Development setup

```sh
sudo apt install build-essential pkg-config \
    libx11-dev libxft-dev libxrender-dev libxext-dev libxrandr-dev libxinerama-dev libfontconfig-dev xvfb libgtest-dev xserver-xephyr
make                  # build (no debugging information)
make DEBUG=1          # build with -Og -g, for gdb, and with TINY_LOG() messages
make check            # unit tests (run against Xvfb)
make help             # all the targets and variables
tools/run-xephyr.sh   # run the window manager in a nested Xephyr
```

Install the git hooks once, they check the formatting, spelling and shell
scripts on every commit and run the static analysis and the tests before a
push:

```sh
sudo apt install pre-commit clang-format clang-tidy cppcheck
pre-commit install --install-hooks
pre-commit install --hook-type pre-push
```

Before sending a change, build without warnings and run `make check`. When
you work on memory handling, also build with the sanitizers:

```sh
make asan        # all the tests with AddressSanitizer and UBSan
```

The same checks run in GitHub Actions (`.github/workflows/ci.yml`) with GCC,
Clang, on Alpine (musl) and NetBSD, plus pre-commit, clang-tidy, cppcheck,
the sanitizers, a coverage minimum and a binary size limit (`make release`
builds the small programs). A
change should pass all of them; the NetBSD job is best effort.

## Tests

Tests live in `tests/`, which mirrors `lib/`: the tests of `lib/wm/dock.cc`
are in `tests/wm/test_dock.cc`. They use googletest, a test suite is named
`<Subject>Test` (`DockTest`, `DockLifeTest`). Tests that need a
display run against Xvfb through `make check`; `tests/helpers.h` and
`tests/fake_client.h` have helpers for fake clients. Every new behavior and every
bug fix needs a test. New files are picked up automatically by the Makefile.
Use `make coverage` to see what is not covered.

Every file in `lib/` is covered by tests at least from 90 %, the ideal is
100 %; the coverage of a file never goes down. A line that cannot be reached
in a test (for example code that runs only in a forked child, whose counters
are lost on `_exit`) is the only acceptable gap, and it is better to keep such
code small.

## Commits

- Small, frequent commits are better than big, rare ones.
- The message is written in English, short, and says what is different from
  the user's point of view: what is new, fixed or changed in behavior. It does
  not describe implementation details.
- Do not mix a refactoring with a change of behavior in one commit.
