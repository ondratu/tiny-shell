#!/bin/sh
# Take the screenshots of the documentation (doc/static/screenshots/).
#
# It is not a part of any build: run it by hand when the look of the shell
# changes much, and commit the pictures. The shell runs in a virtual X server
# (Xvfb) with a configuration of its own in a temporary directory, so the
# settings of the user are not touched.
#
#   tools/screenshots.sh                  # build/ must be built (make)
#   FAVORITES='a.desktop;b.desktop' tools/screenshots.sh
#
# Needs: Xvfb, xdotool and ImageMagick (convert, import), and the programs of
# the favorites for their icons (the desktop files are searched in the data
# directories of the user and of the system, like the shell does).
#
# Variables:
#   FAVORITES  the ids of the desktop files in the dock, from the left
#   TERMINAL   the program in the window of the screenshots (default kitty)
#   SIZE       the size of the screen (default 1440x900)

set -e
cd "$(dirname "$0")/.."
ROOT=$PWD

FAVORITES=${FAVORITES:-"firefox-esr.desktop;kitty.desktop;cz.zeropage.Formiko.desktop;nvim.desktop;cz.zeropage.Reto.desktop"}
TERMINAL=${TERMINAL:-kitty}
SIZE=${SIZE:-1440x900}
DISPLAY_NUMBER=${DISPLAY_NUMBER:-97}
OUT=$ROOT/doc/static/screenshots

for tool in Xvfb xdotool convert import; do
    command -v "$tool" >/dev/null || { echo "$tool is needed" >&2; exit 1; }
done
[ -x build/tiny-shell ] || { echo "run make first" >&2; exit 1; }

WORK=$(mktemp -d)
PIDS=
cleanup() {
    for pid in $PIDS; do kill "$pid" 2>/dev/null || true; done
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

mkdir -p "$OUT" "$WORK/config" "$WORK/shots"
DISPLAY=:$DISPLAY_NUMBER
export DISPLAY
# The language of the dates in the panel, the same on every machine
export LC_ALL=C.UTF-8 LANG=C.UTF-8
export XDG_CONFIG_HOME=$WORK/config
# The icons and the desktop files of the repository first, then the ones of the
# user (the favorites are found there)
export XDG_DATA_DIRS=$ROOT/data:${XDG_DATA_HOME:-$HOME/.local/share}:/usr/local/share:/usr/share
export PATH=$ROOT/build:$PATH
# kitty and other terminals that draw with OpenGL
export LIBGL_ALWAYS_SOFTWARE=1 GDK_BACKEND=x11

Xvfb "$DISPLAY" -screen 0 "${SIZE}x24" >/dev/null 2>&1 &
PIDS="$PIDS $!"
sleep 1

# session STYLE: start the shell with the style and the terminal window
session() {
    printf '[theme]\nstyle = %s\n\n[dock]\nmode = on\nfavorites = %s\n' \
        "$1" "$FAVORITES" >"$XDG_CONFIG_HOME/tiny-shell.ini"
    build/tiny-shell &
    SHELL_PID=$!
    sleep 2
    case $TERMINAL in
    kitty)
        kitty -o remember_window_size=no -o initial_window_width=80c \
            -o initial_window_height=22c -o background=#1e1f22 \
            -o foreground=#d8d8d8 -o font_size=11 -o hide_window_decorations=no \
            sh -c 'printf "$ tiny-shell --version\n$(build/tiny-shell --version)\n$ uname -sm\nLinux x86_64\n$ "; exec sh' \
            >/dev/null 2>&1 &
        ;;
    *)
        "$TERMINAL" >/dev/null 2>&1 &
        ;;
    esac
    APP_PIDS=$!
    sleep 3
}

# finish: end the session (the programs and the shell)
finish() {
    for pid in $APP_PIDS $SHELL_PID; do kill "$pid" 2>/dev/null || true; done
    sleep 1
}

# shoot NAME: a picture of the screen, small as a PNG with few colors
shoot() {
    import -window root "$WORK/shots/$1.png"
    convert "$WORK/shots/$1.png" -colors 160 -strip "PNG8:$OUT/$1.png"
    echo "$OUT/$1.png"
}

# The day: the light style with its wallpaper
session light
shoot desktop-day
xdotool mousemove 18 14 click 1
sleep 1
shoot menu
xdotool key Escape
finish

# The night: the dark style, the default
session dark
shoot desktop-night
xdotool key super
sleep 2
shoot launcher
xdotool key Escape
sleep 1

tiny-fonts >/dev/null 2>&1 &
FONTS=$!
sleep 2
shoot fonts
xdotool key ctrl+q
sleep 1
tiny-settings >/dev/null 2>&1 &
SETTINGS=$!
sleep 2
shoot settings
xdotool key Escape
sleep 1
APP_PIDS="$APP_PIDS $FONTS $SETTINGS"

# Alt+Tab with several applications
tiny-fonts >/dev/null 2>&1 &
APP_PIDS="$APP_PIDS $!"
sleep 1
tiny-about >/dev/null 2>&1 &
APP_PIDS="$APP_PIDS $!"
sleep 2
xdotool keydown alt key Tab
sleep 1
xdotool key Tab
sleep 1
shoot switcher
xdotool keyup alt
finish
