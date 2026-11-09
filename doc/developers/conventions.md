# Technology and conventions



The layout of the directories is described in
[Layout of the sources](code-layout.md).

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
