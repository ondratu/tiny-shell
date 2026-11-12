# Dependencies



- Allowed without discussion: libX11, Xft, Xrender, fontconfig and FreeType (Xft
  uses them; we use fontconfig for the list of fonts and FreeType to read the
  `name` table of a font file), libXext (the SYNC extension for resizing
  without tearing and for the idle time, DPMS for the power saving of the display) and, when needed for a feature, Xrandr or other extensions
  that are on every Xorg system.
- Everything else needs a discussion first and must be **optional at build
  time** (a make variable, with the feature compiled out when it is off).
  Lua for scripting is the planned example.
- Optional at build time, now: **libpam** (the make variable `PAM=auto|yes|no`;
  `auto` uses it when `<security/pam_appl.h>` and `-lpam` are found, there is no
  `pkg-config` file on every system). Only `tiny-lock-auth` of the
  [screen lock](screen-lock.md) and the example `examples/password.cc` use it;
  the shell and `tiny-lock` do not link it. Without PAM the password goes to a
  program that checks it: `unix_chkpwd` of Linux-PAM (`make UNIX_CHKPWD=/path`
  sets where it is) or `tiny-lock-pwcheck`.
- **crypt()** (libcrypt: in the C library of musl and the BSD systems, a library
  of its own with glibc, which comes with its development files). Only
  `tiny-lock-pwcheck` links it, to compare a password with a hash.
- Programs of the system that are run, not linked: `xrandr` for the position of
  the secondary monitor and `setxkbmap` for the layouts of the keyboard (the
  compilation of a keymap is `xkbcomp`'s work and is not worth a library of
  ours). They are used only when the settings ask for it, and a message says
  when one is missing.
- Prefer a small vendored single-file library (public domain or permissive,
  e.g. `stb_image.h`) over a new shared library.
- Vendored libraries live in `lib/third_party/` unchanged, with their licence
  and the version in `lib/third_party/README.md`. They are compiled as system
  headers (`-isystem`), skipped by the formatter, the linter and the
  spell checker, and not counted in the coverage. Update them by copying the
  new files and the version, never by editing.
- Keep binary size and memory use in mind; do not add dependencies only for
  convenience.
