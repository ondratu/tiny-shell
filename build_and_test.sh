#!/bin/sh

make tiny-shell || exit 1

#xinit ./xinitrc -- /usr/bin/Xephyr :100 -screen 1024x768 -host-cursor -terminate
xinit ./xinitrc -- /usr/bin/Xephyr :100 -screen 800x600 -host-cursor -terminate
