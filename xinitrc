# xinitrc
export GDK_BACKEND=x11

xclock &
xeyes &
xterm &

exec ./src/tiny-shell
