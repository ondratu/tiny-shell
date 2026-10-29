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
2. **Few dependencies.** The shell runs with libX11, Xft and Xrender (and the
   C and C++ standard libraries). See [Dependencies](#dependencies).
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

- **Language:** C++17. Use the standard library, but keep templates, exceptions
  and RTTI simple; the code is read by people learning Xlib. Do not use
  C++ exceptions for control flow in the event loop.
- **Build system:** GNU make, one `Makefile`. Dependencies are found with
  `pkg-config`. No autotools or CMake.
- **Code style:** 4 spaces, braces on the same line, `CamelCase` types,
  `snake_case` functions and members, `#pragma once`, one class per header
  where it is practical. Match the surrounding code, including the density of
  comments. Namespaces: `tiny` for the toolkit (`lib/`), `wm` for the window
  manager and the shell.
- **Layout:** `lib/` is the library `libtiny.a` (toolkit and window manager),
  `src/main.cc` is the window manager itself, `tests/` are the unit tests,
  `examples/` are small toolkit programs (`make examples`) and a default
  session, `tools/` are development scripts. Everything is built into
  `build/`. Files that belong to one machine (a session script, local
  settings) are not committed: they live in the ignored `local/`, created
  from `examples/` by the scripts.
- **Geometry is `int`.** Sizes, offsets and coordinates are `int`, like in
  Xlib structures, so that sums and differences do not wrap around. The
  size of a widget cannot be negative: the constructor and `resize()` clamp
  it to zero. Where an X request takes an unsigned value, convert with
  `tiny::x_dim()` (a window size, limited to 1..65535) or `tiny::x_uint()`
  (other values, never negative).
- **Strictness:** the code compiles without warnings with GCC and Clang, with
  a long list of warnings on (see `WARNINGS` in the Makefile), is formatted by
  `clang-format` (`make format`) and checked by `clang-tidy` (`make lint`). Fix the cause of a warning; if
  it really cannot be fixed (a macro or a header of Xlib), isolate it in one
  small place with a comment. Test coverage stays as high as is sensible and
  never goes down.

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
- **No global state in new code.** The display, the theme and the handler
  table are global today; new code takes what it needs as an argument, so that
  it can be tested and the globals can be removed later.
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

- Allowed without discussion: libX11, Xft, Xrender, and (when needed for a
  feature) Xrandr or Xext extensions that are on every Xorg system.
- Everything else needs a discussion first and must be **optional at build
  time** (a make variable, with the feature compiled out when it is off).
  Lua for scripting is the planned example.
- Prefer a small vendored single-file library (public domain or permissive,
  e.g. `stb_image.h`) over a new shared library.
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
- One look, scalable for high-DPI (`TINY_SCALE_FACTOR`). Every size in code
  comes from the theme, never from a literal number.
- Robustness: a misbehaving client must not crash the window manager.

## Development setup

```sh
sudo apt install build-essential pkg-config \
    libx11-dev libxft-dev libxrender-dev xvfb libgtest-dev xserver-xephyr
make                  # build
make check            # unit tests (run against Xvfb)
tools/run-xephyr.sh   # run the window manager in a nested Xephyr
```

Install the git hooks once, they check the formatting, spelling and shell
scripts on every commit and run the static analysis and the tests before a
push:

```sh
sudo apt install pre-commit clang-format clang-tidy
pre-commit install --install-hooks
pre-commit install --hook-type pre-push
```

Before sending a change, build without warnings and run `make check`. When
you work on memory handling, also build with the sanitizers:

```sh
make asan        # all the tests with AddressSanitizer and UBSan
```

The same checks run in GitHub Actions (`.github/workflows/ci.yml`) with GCC,
Clang (also with libc++), on Alpine (musl) and FreeBSD, plus pre-commit,
clang-tidy, the sanitizers, a coverage minimum and a binary size limit. A
change should pass all of them; the FreeBSD job is best effort.

## Tests

Tests live in `tests/test-<name>.cc` and use googletest. Tests that need a
display run against Xvfb through `make check`; `tests/helpers.h` and
`tests/client.h` have helpers for fake clients. Every new behavior and every
bug fix needs a test. New files are picked up automatically by the Makefile.
Use `make coverage` to see what is not covered.

## Commits

- Small, frequent commits are better than big, rare ones.
- The message is written in English, short, and says what is different from
  the user's point of view: what is new, fixed or changed in behavior. It does
  not describe implementation details.
- Do not mix a refactoring with a change of behavior in one commit.
