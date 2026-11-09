# For developers

Thanks for your interest! These pages describe the core decisions of the
project. Everyone is expected to follow them; when a change needs to break
one, discuss it first and update the page together with the change. The short
version for GitHub is [CONTRIBUTING.md](https://github.com/ondratu/tiny-shell/blob/master/CONTRIBUTING.md).

| Page | What is in it |
| ---- | ------------- |
| [Technology and conventions](conventions.md) | C++17, make, the code style, geometry, strictness |
| [Layout of the sources](code-layout.md) | what is in `lib/`, `src/`, `data/`, `tests/` |
| [Architecture](architecture.md) | layers, the event loop, actions, themes, design principles |
| [Dependencies](dependencies.md) | what may be linked and how a new dependency gets in |
| [User interface guidelines](ui-guidelines.md) | how menus, buttons and the pointer behave |
| [Development setup and tests](testing.md) | building, the tests, coverage, CI |

The reference documentation of the widget toolkit and of the Lua plugins of the
panel will be added here when the plugins exist (see the [roadmap](../roadmap.md)).

## Language

Everything in the repository is in English: source code, comments, commit
messages, issues and documentation.

## Goals that decide everything else

1. **Small.** Small source, small binary, small memory footprint. A feature
   that costs a lot of any of them needs a good reason.
2. **Few dependencies.** The shell runs with libX11, Xft, Xrender and Xext, and
   fontconfig and FreeType that Xft needs anyway (and the C and C++ standard
   libraries).
   See [Dependencies](dependencies.md).
3. **Standards first.** Use ICCCM, EWMH and freedesktop.org specifications as
   written. Applications and external tools must work without being told
   anything about tiny-shell. Do not invent private protocols when a
   standard one exists.
4. **Works on embedded systems with Xorg.** No D-Bus, systemd, GLib or GPU
   features are required. Everything extra is optional and off by default.
5. **Correct before featureful.** A window manager that behaves properly with
   ordinary applications matters more than a new widget.

Out of scope (see [What it is not](../index.md#what-it-is-not)): a desktop environment, a compositor, Wayland.

## Commits

- Small, frequent commits are better than big, rare ones.
- The message is written in English, short, and says what is different from
  the user's point of view: what is new, fixed or changed in behavior. It does
  not describe implementation details.
- Do not mix a refactoring with a change of behavior in one commit.
