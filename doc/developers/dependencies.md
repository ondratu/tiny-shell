# Dependencies



- Allowed without discussion: libX11, Xft, Xrender, fontconfig and FreeType (Xft
  uses them; we use fontconfig for the list of fonts and FreeType to read the
  `name` table of a font file), libXext (the SYNC extension for resizing
  without tearing) and, when needed for a feature, Xrandr or other extensions
  that are on every Xorg system.
- Everything else needs a discussion first and must be **optional at build
  time** (a make variable, with the feature compiled out when it is off).
  Lua for scripting is the planned example.
- Optional at build time, now: **libpam** (the make variable `PAM=auto|yes|no`;
  `auto` uses it when `<security/pam_appl.h>` and `-lpam` are found, there is no
  `pkg-config` file on every system). Only the example `examples/password.cc`
  uses it, to try how a password is checked; the shell does not link it.
  Without PAM that example runs a helper that checks the password of the user
  from its standard input (`unix_chkpwd` of Linux-PAM, `make UNIX_CHKPWD=/path`
  sets where it is).
- Prefer a small vendored single-file library (public domain or permissive,
  e.g. `stb_image.h`) over a new shared library.
- Vendored libraries live in `lib/third_party/` unchanged, with their licence
  and the version in `lib/third_party/README.md`. They are compiled as system
  headers (`-isystem`), skipped by the formatter, the linter and the
  spell checker, and not counted in the coverage. Update them by copying the
  new files and the version, never by editing.
- Keep binary size and memory use in mind; do not add dependencies only for
  convenience.
