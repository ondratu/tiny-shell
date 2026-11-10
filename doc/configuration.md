# Configuration

Everything that can be set, and what to do to set it.

## The configuration file

The settings are in an ini file, `tiny-shell.ini`, read from two places and
layered: the global one in `$XDG_CONFIG_DIRS` (`/etc/xdg` by default; set the
variable to `/usr/local/etc/xdg` where the system keeps its files there, e.g. on
the BSDs) and then the one of the user in `$XDG_CONFIG_HOME` (`~/.config` by
default). A key of the user's file replaces the same key of the global file, so
it only needs the keys that differ. Both files are optional, `tiny-shell --info`
lists them and the settings in use. The file is read when tiny-shell starts and
again when it gets `SIGHUP` or is run as `tiny-shell --reload` (see [the command line](#command-line)); a
line or a value that is wrong is reported on stderr and the default is used.

```ini
# A comment is a whole line that starts with # or ; (not the end of a line)
[theme]
# A number above 0, up to 16
scale = 1.5
# The font of the shell: a family (see tiny-fonts for the names) or a list of
# them, separated by commas; the next ones are used for the characters that the
# first one does not have, and when it is not installed
font = DejaVu Sans,sans
# The size in points at the scale 1, from 4 to 96
font_size = 12
# The colors: dark (the default) or light
style = dark
# The style of the font, a name that the family has ("Bold", "Italic"); empty
# is the default style. The title bars are always bold.
font_style = Bold
# Tell GTK and Qt programs the theme, the font and the DPI (XSETTINGS, see
# below): yes (the default) or no
xsettings = yes

[xft]
# How the programs draw text, put into the X resources Xft.* when tiny-shell
# starts. Empty (or no key) means "not set": the system decides.
# yes or no
antialias = yes
# none, slight, medium or full
hinting = slight
# The order of the subpixels of the display: none, rgb, bgr, vrgb or vbgr
rgba = rgb

[wallpaper]
# A PNG or JPEG file; empty is no wallpaper, no key is the wallpaper of the
# style (see Wallpaper below). ~/ is the home.
file = ~/Pictures/mountains.jpg
# fill (the default), center or tile
mode = fill

[commands]
# Restart and Shutdown in the main menu (these are the defaults)
reboot = systemctl reboot
poweroff = systemctl poweroff
# Runs the applications with Terminal=true, the command follows it; the
# Terminal item of the main menu starts the program alone (xterm)
terminal = xterm -e
# Super + L and the Lock Screen item of the main menu; see The lock below
lock = tiny-lock

[lock]
# Minutes without input to a black screen: 1 (the default), 5, 10, 15 or never
blank_after = 1
# Minutes without input that lock the screen: 5, 10, 15, 30, 60 or never (the
# default)
idle_after = never
# How tiny-lock-auth checks the password: auto (the default), pam, checker or
# pwcheck
auth = auto
# The PAM service; a service with no file in /etc/pam.d uses "other"
pam_service = tiny-lock

[panel]
# The clock at the right end of the panel: a format of strftime(); empty is
# no clock. The names of the days and the order of the date follow LC_TIME / LANG.
# It changes at the start of every minute (the default is below)
clock_format = %a %e %B %R

# The icons of programs (the system tray) in the panel, left of the clock:
# yes (the default) or no. Set no when another tray (stalonetray, trayer) runs.
system_tray = yes

[monitors]
# The monitors are numbered from the left (and from the top when they start at
# the same place). The monitor of the panel, the dock, the launcher and the
# dialogs: auto (the primary monitor of RandR), a number, or the name of an
# output (DP-1)
primary = auto
# The monitor that placement moves: auto (the first one that is not the
# primary), a number or the name of an output
secondary = auto
# Where the secondary monitor is, seen from the primary one: keep (as the
# system has it), right, left, above or below. It runs xrandr.
placement = keep

[dock]
# When the dock is shown: autohide (the default: it is hidden while the active
# window covers it, and the pointer at the bottom edge of the screen shows it
# until the pointer leaves the dock), on (always) or off (never)
mode = autohide
# Applications that stay in the dock when they do not run: the ids of their
# desktop files, separated by ;. The menu of the dock writes it (see [Using the shell](usage.md#the-dock-and-the-favorite-applications))
favorites = firefox.desktop;org.gnome.Nautilus.desktop
```

[examples/tiny-shell.ini](https://github.com/ondratu/tiny-shell/blob/master/examples/tiny-shell.ini) is a file to start with. Use
`sudo reboot`, `doas poweroff` or similar where there is no systemd.

tiny-shell writes the file of the user (never the global one) when you change
the favorites of the dock, and so does `tiny-settings` (see [Applications](applications.md#settings)). It makes the
whole file again from the values that were in it, so **your comments are lost**
and the sections come in the order of their names; the values of the global file
are not copied into it. The new file replaces the old one in one step, and a
failure is reported on stderr and leaves the old file as it was. The file says
so at the top.

## The lock

`[commands] lock` is the command that Super + L and the Lock Screen item run;
the default is `tiny-lock`, the screen lock of tiny-shell
(see [Screen lock](lock.md)). Another locker (`slock`, `i3lock`) can be set
here.

`[lock]` is read by `tiny-lock`:

| Key | Value | What it says |
| --- | ----- | ------------ |
| `blank_after` | `1`, `5`, `10`, `15`, `never` | minutes without input in the screen with the time to a black screen with no text; 1 is the default |
| `idle_after` | `5`, `10`, `15`, `30`, `60`, `never` | minutes without input (in any program) after which tiny-shell locks the screen; `never` is the default |
| `auth` | `auto`, `pam`, `checker`, `pwcheck` | how the password is checked; `auto` takes PAM where it can work, then `unix_chkpwd`, then `tiny-lock-pwcheck` |
| `pam_service` | a name | the PAM service, `tiny-lock` by default; no file is needed for it |
| `checker` | a path | the program like `unix_chkpwd`, when it is not where the system keeps it |
| `pwcheck` | a path | `tiny-lock-pwcheck`, when it is not next to `tiny-lock` |

A wrong value is reported on stderr and the default is kept. A change is read
the next time the screen is locked; `idle_after` is read by tiny-shell itself, so
it needs a restart or `tiny-shell --reload`.

## The clock

The date and the time are at the right end of the panel. The text is made by
`strftime()` from `[panel] clock_format`, so the names of the days and months
and the order of the date follow the language of the session (`LC_TIME`, set by
`LANG` or `LC_ALL`): `%e` is the day, `%B` the name of the month (`čt 5
listopadu 15:33` in Czech, `Thu 5 November 15:33` in English) and `%R` is
`HH:MM`. The default is `%a %e %B %R`, without the year. The clock is as wide as
the widest text of the format in a year, so the panel does not move, and it
changes at the start of every minute, so it does not show seconds. An empty
format removes the clock.

## Settings for GTK and Qt (XSETTINGS)

tiny-shell is the manager of settings of the screen, the way the
[XSETTINGS](https://specifications.freedesktop.org/xsettings-spec/latest/)
specification describes it: it owns the selection `_XSETTINGS_S0`, tells the
programs with the `MANAGER` message and keeps the settings in the property
`_XSETTINGS_SETTINGS` of its window. The programs that follow it (GTK 2 and 3,
Qt with the GTK platform theme, Firefox...) change at once when a setting
changes. The settings come from the configuration file:

- `Net/ThemeName`: `Adwaita-dark` for `[theme] style = dark`, `Adwaita` for
  light (GTK 2 and 3 can choose a dark theme only by its name; GTK 4 and
  libadwaita read the color scheme of the portal instead, not done yet)
- `Gtk/FontName`: the first family of `[theme] font`, `font_style` and
  `font_size`, like "DejaVu Sans Bold 12"
- `Xft/DPI`, `Gdk/UnscaledDPI` and `Gdk/WindowScalingFactor`: 96 dpi times
  `[theme] scale`; the whole part of the scale is the factor of the windows
- `Xft/Antialias`, `Xft/Hinting`, `Xft/HintStyle` and `Xft/RGBA` from `[xft]`
  (what is not set is not published)

A settings daemon that already runs (`gsd-xsettings`, `xsettingsd`) is left
alone and tiny-shell publishes nothing; the same happens with `[theme] xsettings
= no`. The cursor theme is not published yet.

## The system tray

The icons of programs (a network applet, a volume control, a messenger) are in
the panel, left of the clock, the newest one at the left end. tiny-shell is the
tray when it starts, the way the [System Tray
Protocol](https://specifications.freedesktop.org/systemtray-spec/latest/) and
[XEMBED](https://specifications.freedesktop.org/xembed-spec/latest/) say: it
owns the selection `_NET_SYSTEM_TRAY_S0`, tells the programs with the `MANAGER`
message, and a program asks for an icon with `_NET_SYSTEM_TRAY_OPCODE`. An icon
that the program hides (`_XEMBED_INFO`) takes no place, and one that the program
destroys is gone from the panel. The texts of balloons (`BEGIN_MESSAGE`) are not
shown.

`[panel] system_tray = no` turns the tray off. Another tray that already runs is
left alone (there is a message and no tray in the panel), and the tray stops
when another one takes the selection; the icons go back to their programs, which
ask again when a tray announces itself.

`make examples` builds `build/examples/tray_icon`, a program with one icon in
the tray and a menu with Quit. It draws one of the icons of the shell, so start
it with the data of the repository (`tools/run-xephyr.sh` already has it in
`XDG_DATA_DIRS`):

```sh
XDG_DATA_DIRS=$PWD/data:/usr/share build/examples/tray_icon
```

## Wallpaper

`build/tiny-wallpaper [-m fill|center|tile] [-s dark|light] [FILE]` draws a
picture as the background of the root window. Without arguments it uses
`[wallpaper] file` and `mode` of the configuration, so `tiny-shell` starts it
that way when a file is set, and again each time the size of the screen changes
(RandR, a new monitor): the window manager only watches the size of the root
window, it does not link the decoders. When the configuration has no
`[wallpaper] file` (an empty one means no wallpaper), the wallpaper is the one
that `make install` puts in `share/backgrounds/tiny-shell`: the landscape with
the fox (`tiny-shell-landscape-fox-1920x1080.png`) for the light style and the
night (`tiny-shell-night-1920x1080.png`) for the dark one. It changes with the
style (also with the actions `theme-dark`, `theme-light` and `theme-toggle`: the
shell starts the program again with `-s`); a wallpaper that is set in the file
does not. The modes:

- **fill** (the default): the picture is scaled to cover the screen and the part
  that sticks out is cut, from all the sides equally;
- **center**: not scaled, in the middle of the screen, black around it (or cut
  when it is bigger);
- **tile**: not scaled, repeated from the top left corner.

The formats are PNG and JPEG (baseline and progressive; the kind of the file is
found by its content, not by the extension). A transparent part is shown black.
A picture of more than 25 million pixels (a 5K screen has 14.7) or a file over
64 MB is refused with a message on stderr. The picture is decoded, drawn and
freed: the program does not stay in memory. The pixmap stays in the X server as
`_XROOTPMAP_ID` and `ESETROOT_PMAP_ID`, as `Esetroot` leaves it, and the next
run frees the old one. A screen of several monitors is one picture over all of
them. The picture does not change with the light and dark variants of the theme
when it is set in the file (the plain color does). The path must be absolute (or
start with `~/`), because the program starts in another directory. tiny-shell
watches the size of the screen only when a wallpaper is set in the file when it
starts, so the first wallpaper that you set in `tiny-settings` is drawn at once,
but it follows a change of the screen after the next login. A file that cannot
be read is reported on stderr and the plain color stays.

The decoder of photos is a second copy of `stb_image` in
`lib/third_party/vendor_photo.cc`, with only PNG and JPEG on: it is linked into
`tiny-wallpaper` only, so `tiny-shell` does not carry the JPEG code.

## Multiple monitors

The monitors are the ones of RandR (`XRRGetMonitors`, also the ones that `xrandr
--setmonitor` makes, e.g. to split a wide monitor). When RandR knows only one,
the screens of Xinerama are used (a nested Xephyr started with `+xinerama` has
them only there; such monitors have no names, so the position cannot be
changed). They are numbered from the left to the right, and from the top to the
bottom when two start at the same place; a monitor that shows the same picture
as another (a clone) is not counted. Without RandR the whole screen is one
monitor.

- **The shell is on the primary monitor.** `[monitors] primary` chooses it by
  its number or the name of its output; `auto` is the primary monitor of RandR
  (`xrandr --primary`), or the first one. The panel, the dock (and the strip
  that shows it), the launcher, the run dialog and the window switcher are
  there. A dock on every monitor is not done (the dock is one widget with one
  list of windows).
- **Windows.** A window opens where the pointer is when it has no place of its
  own, is maximized to the work area of the monitor it is on (the monitor that
  has most of the frame), and fullscreen covers that monitor only. The panel
  takes the top of the primary monitor, the struts of other docks reduce the
  monitors they touch: a strut is measured from the edge of the whole screen (as
  `_NET_WM_STRUT` says), so a top strut does not touch a monitor that is below
  another one. `_NET_WORKAREA` is the work area of the primary monitor.
- **Position.** `[monitors] placement` puts the secondary monitor to the
  `right`, `left`, `above` or `below` the primary one by running `xrandr
  --output SECONDARY --right-of PRIMARY` (the program `xrandr` must be
  installed). With `keep` (the default) the layout of the system is left as it
  is. The command is run again when the monitors change, only when the placement
  is not as asked, and not twice in a row. `secondary` must be the name of an
  output for it to work (the monitors that `--setmonitor` made are not outputs).
- **Numbers.** Only `tiny-settings` shows them: while the section Displays is
  shown, the number, the name and the size of every monitor are in its corner,
  over all the windows (the primary one says so), and they go away when the
  section is left or the settings are closed (the settings ask the shell for it
  with the client message `_TINY_SHELL_MONITORS`).

Save in `tiny-settings` makes the shell read `[monitors]` again at once (the
same message), so the primary monitor and the position are changed without a new
login; the other settings still need one. A monitor that is plugged in or out,
or moved by `xrandr`, is used at once. The position needs a server with more
than one output: a nested X server like Xephyr has a single one, so there is
nothing to move there (try it on a real display, or `xrandr --listmonitors`
shows what the shell sees).

## Where the launcher looks for applications

The launcher reads the `.desktop` files from the `applications` directory of
each of these, the first one wins when an application is in more of them:

1. `$XDG_DATA_HOME` (`~/.local/share` when it is not set)
2. the directories of `$XDG_DATA_DIRS`, in their order
   (`/usr/local/share:/usr/share` when it is not set)
3. `/usr/local/share`, if the list above does not have it

The last step is the only thing added on top of the XDG specification, the same
way as GNOME does it; you can move `/usr/local/share` to any place by listing it
in `XDG_DATA_DIRS`. Other prefixes (for example `/usr/pkg/share` on NetBSD)
belong to the session, so set them in `~/.xinitrc`.

The launcher starts a program with the `PATH` of tiny-shell, so put
`~/.local/bin` and the like to `PATH` in the session too.

## Command line

```
tiny-shell [--replace] [--info] [--reload] [--version] [--help]
```

`--replace` takes over from a window manager that is already running, the way
ICCCM describes it (the `WM_S0` selection): the old one is asked to leave and
tiny-shell starts when it has gone. Another window manager started with its own
`--replace` takes over from tiny-shell in the same way. Window managers that do
not use the selection (for example twm) have to be stopped first. `--info`
prints what tiny-shell is and which settings it uses. `--reload` tells the
tiny-shell that runs on the display to read the configuration files again; it is
the same as `kill -HUP` of its process (the process is found through
`_NET_WM_PID` of the window `_NET_SUPPORTING_WM_CHECK`, so it works on the
computer where tiny-shell runs). What changes at once: the colors (`[theme]
style`), the favorites, the commands, the wallpaper, the clock, the system tray,
the mode of the dock, `[xft]` and what the programs are told by XSETTINGS. The
scale and the font of the shell itself need a restart (a message says so).
`SIGTERM` and `SIGINT` end tiny-shell.
