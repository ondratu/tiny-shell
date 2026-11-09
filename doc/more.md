# More

## The project

- The source code and the issues: [github.com/ondratu/tiny-shell](https://github.com/ondratu/tiny-shell)
- [Report a problem or ask for a feature](https://github.com/ondratu/tiny-shell/issues)
- [The roadmap](roadmap.md) and [what was left out on purpose](https://github.com/ondratu/tiny-shell/blob/master/wont.md)
- [How to contribute](developers/index.md)
- License: [BSD 2-Clause](https://github.com/ondratu/tiny-shell/blob/master/LICENSE)

## The standards that tiny-shell follows

- [ICCCM](https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html),
  the conventions between a client and the window manager
- [EWMH](https://specifications.freedesktop.org/wm-spec/latest/), the window
  manager hints of freedesktop.org
- [XSETTINGS](https://specifications.freedesktop.org/xsettings-spec/latest/),
  settings for the toolkits
- [System Tray Protocol](https://specifications.freedesktop.org/systemtray-spec/latest/)
  and [XEMBED](https://specifications.freedesktop.org/xembed-spec/latest/)
- [Desktop Entry](https://specifications.freedesktop.org/desktop-entry-spec/latest/),
  [Icon Theme](https://specifications.freedesktop.org/icon-theme-spec/latest/) and
  [Icon Naming](https://specifications.freedesktop.org/icon-naming-spec/latest/)
- [Startup Notification](https://specifications.freedesktop.org/startup-notification-spec/latest/)
- [Xlib manual](https://www.x.org/releases/current/doc/libX11/libX11/libX11.html)

## Libraries

tiny-shell links Xlib, Xft, Xrender and Xext, and carries the single-file
libraries `stb_image` and `nanosvg` (see
[Dependencies](developers/dependencies.md)). The tests use
[googletest](https://github.com/google/googletest).
