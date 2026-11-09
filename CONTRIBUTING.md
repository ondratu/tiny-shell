# Contributing to tiny-shell

Thanks for your interest! This document lists the core decisions of the
project. Everyone is expected to follow them; when a change needs to break
one, discuss it first and update the documentation together with the change.

The details are in the developer documentation, in [doc/developers/](doc/developers/index.md)
(it is also published on the web site of the project):

- [Technology and conventions](doc/developers/conventions.md): C++17, GNU make,
  the code style, geometry, strictness
- [Layout of the sources](doc/developers/code-layout.md)
- [Architecture](doc/developers/architecture.md): layers, the event loop,
  actions, themes, design principles
- [Dependencies](doc/developers/dependencies.md): what may be linked
- [User interface guidelines](doc/developers/ui-guidelines.md)
- [Development setup and tests](doc/developers/testing.md): building, tests,
  coverage, CI, the web site

## Language

Everything in the repository is in English: source code, comments, commit
messages, issues and documentation.

## Goals that decide everything else

1. **Small.** Small source, small binary, small memory footprint. A feature
   that costs a lot of any of them needs a good reason.
2. **Few dependencies.** The shell runs with libX11, Xft, Xrender and Xext, and
   fontconfig and FreeType that Xft needs anyway (and the C and C++ standard
   libraries). See [Dependencies](doc/developers/dependencies.md).
3. **Standards first.** Use ICCCM, EWMH and freedesktop.org specifications as
   written. Applications and external tools must work without being told
   anything about tiny-shell. Do not invent private protocols when a
   standard one exists.
4. **Works on embedded systems with Xorg.** No D-Bus, systemd, GLib or GPU
   features are required. Everything extra is optional and off by default.
5. **Correct before featureful.** A window manager that behaves properly with
   ordinary applications matters more than a new widget.

Out of scope (see the [README](README.md)): a desktop environment, a compositor,
Wayland.

## Before you send a change

- Build without warnings and run `make check` (see
  [Development setup and tests](doc/developers/testing.md)).
- Every new behavior and every bug fix needs a test; the coverage of a file
  never goes down.
- Code is formatted by `clang-format` and checked by `clang-tidy` and
  `cppcheck`; `pre-commit install` sets the hooks.
- Documentation is in `doc/` and the web site is built from it
  (`python3 doc/build.py`); a change of behavior changes the page that
  describes it.

## Commits

- Small, frequent commits are better than big, rare ones.
- The message is written in English, short, and says what is different from
  the user's point of view: what is new, fixed or changed in behavior. It does
  not describe implementation details.
- Do not mix a refactoring with a change of behavior in one commit.
