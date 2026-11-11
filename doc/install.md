# Installation

How to build, install and try tiny-shell, and on which systems it runs.

## Platforms

| Platform | Status                                         |
| -------- | ---------------------------------------------- |
| Linux (glibc) | Primary target, with Xorg; built and tested in CI with GCC and Clang |
| Linux (musl)  | Alpine Linux, built and tested in CI |
| NetBSD        | Built in CI on a best effort basis |
| Other BSD     | Should work (X11 only), not tested |
| Wayland       | Not planned, X11 on Xorg only |

tiny-shell needs nothing from the system but the X libraries: no D-Bus, no
systemd, no GLib and no GPU features.

## Building

Requirements: a C++17 compiler, GNU make, `pkg-config` and the development files
of `x11`, `xft`, `xrender` and `xext`, and `xrandr` and `xinerama` for several
monitors (they are used when they are installed; `make RANDR=0 XINERAMA=0`
builds without them and the whole screen is one monitor).

```sh
sudo apt install build-essential pkg-config \
    libx11-dev libxft-dev libxrender-dev libxext-dev libxrandr-dev \
    libxinerama-dev libfontconfig-dev

make                 # build/tiny-shell, tiny-settings, tiny-fonts, tiny-about, tiny-wallpaper, tiny-lock
make release         # the same, small and stripped, into build-release/
make DEBUG=1         # into build-debug/, with the debugging information (-Og -g), much bigger,
                     # and with diagnostic messages on stderr (TINY_LOG)
```

The names of the packages on other systems:

| System | Packages |
| ------ | -------- |
| Fedora | `gcc-c++ make pkgconf libX11-devel libXft-devel libXrender-devel libXext-devel libXrandr-devel libXinerama-devel fontconfig-devel` |
| Alpine | `build-base pkgconf libx11-dev libxft-dev libxrender-dev libxext-dev libxrandr-dev libxinerama-dev fontconfig-dev` |

PAM is optional: with `libpam0g-dev` (Debian and Ubuntu), `pam-devel` (Fedora) or
`linux-pam-dev` (Alpine) installed, `make` builds the screen lock with it
(`make PAM=no` does not, `make PAM=yes` insists on it). See [Screen lock](lock.md).

## Installation

```sh
sudo make install                 # into /usr/local
make install PREFIX=/usr DESTDIR=/tmp/stage   # a package: /tmp/stage/usr/...
sudo make uninstall               # the same PREFIX
```

It installs the programs into `bin/` and the contents of `data/` into `share/`.
No program gets a special mode: where the screen lock needs `tiny-lock-pwcheck`
to read the hash of the password (no PAM, or a BSD system), run `sudo make
install-setuid`, see [Screen lock](lock.md#tiny-lock-pwcheck-and-its-rights).
The files:

- `share/tiny-shell/icons/white` and `black`: the symbolic icons of the shell.
  The shell paints them with the color of the text, so it reads the `white`
  ones; the `black` ones are for other programs. The Icon Naming Specification
  names are used (`start-here` is the logo, `window-close`, `view-app-grid`,
  `application-x-executable` is the icon of an application that has none), and
  four names are the shell's own: `window`, `window-shade`, `window-switch` and
  `workspace-switcher`. Without the files the shell draws the symbols as before.
  `tools/icons/gen_icons.py` makes the set.
- `share/icons/hicolor/scalable/apps`: `tiny-shell`, `tiny-settings` and
  `tiny-fonts`, the icons of the windows of the programs.
- `share/applications`: the desktop files of `tiny-settings`, `tiny-fonts` and
  `tiny-about`, so that the launcher lists the first two and the dock finds the
  icons of their windows (`StartupWMClass`).
- `share/backgrounds/tiny-shell`: the wallpapers, the fox by day and the fox
  by night (JPEG, 1920x1080), which are the defaults of the light and the dark
  style.

The files are searched in `$XDG_DATA_HOME`, `$XDG_DATA_DIRS` and
`/usr/local/share`. To try the shell from the build directory with the data from
the repository, run it with `XDG_DATA_DIRS=$PWD/data:/usr/share` and `build` in
the `PATH` (the script `tools/run-xephyr.sh` does it).

## Trying it

The safest way is a nested X server. The script builds the window manager and
starts it in `Xephyr` with a session from `local/xinitrc`, which it creates from
[examples/xinitrc](https://github.com/ondratu/tiny-shell/blob/master/examples/xinitrc) on the first run (`local/` is not in git,
so you can change it freely; see the script for `local/env.sh`):

```sh
tools/run-xephyr.sh
```

To use tiny-shell as your real window manager, run `exec build/tiny-shell` at
the end of your `~/.xinitrc`. Set `scale` (for example `1.5` or `2`) in the
configuration file to scale the theme on a high-DPI screen.
