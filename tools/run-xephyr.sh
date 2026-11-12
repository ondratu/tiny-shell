#!/bin/sh
# Build tiny-shell and run it in a nested X server (Xephyr).
#
# Local settings (not in git) are in local/:
#   local/xinitrc  the session, created from examples/xinitrc when missing
#   local/env.sh   shell variables, e.g. XEPHYR_SCREEN=2048x1536

set -e
cd "$(dirname "$0")/.."

XEPHYR_DISPLAY=:100
XEPHYR_SCREEN=1024x768
# shellcheck source=/dev/null
[ -f local/env.sh ] && . ./local/env.sh

mkdir -p local
[ -f local/xinitrc ] || cp examples/xinitrc local/xinitrc

make
TINY_SHELL=$PWD/build/tiny-shell
export TINY_SHELL

# The icons, desktop files and wallpapers of the repository, and the programs
# that the desktop files start, without installing them
XDG_DATA_DIRS="$PWD/data:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
PATH="$PWD/build:$PATH"
export XDG_DATA_DIRS PATH

# The nested session is a tiny-shell one, not the desktop of the terminal that
# started it (see the autostart in doc/session.md)
XDG_CURRENT_DESKTOP=tiny-shell
export XDG_CURRENT_DESKTOP

xinit ./local/xinitrc -- "$(command -v Xephyr)" "$XEPHYR_DISPLAY" \
    -screen "$XEPHYR_SCREEN" -host-cursor -terminate
