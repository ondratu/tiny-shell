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

make build/tiny-shell
TINY_SHELL=$PWD/build/tiny-shell
export TINY_SHELL

xinit ./local/xinitrc -- "$(command -v Xephyr)" "$XEPHYR_DISPLAY" \
    -screen "$XEPHYR_SCREEN" -host-cursor -terminate
