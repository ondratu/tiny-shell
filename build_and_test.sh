#!/bin/sh

(cd src && make tiny-shell || exit 1)

xinit ./xinitrc -- /usr/bin/Xephyr :100 -screen 1024x768 -host-cursor
