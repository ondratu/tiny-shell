# Using the shell

What the panel, the dock, the launcher and the windows do, and the keys that go with them.

## The panel

The top panel has the logo at the left, which opens the main menu, the system
tray and the date and time at the right end (see [The clock](configuration.md#the-clock)
and [The system tray](configuration.md#the-system-tray)).

## Keyboard shortcuts

| Keys | What it does |
| ---- | ------------ |
| Super | the full screen launcher with search |
| Alt+F2 | the run dialog; Tab completes the name of a program from `PATH` |
| Alt+Tab, Alt+Shift+Tab | the window switcher |
| Alt+; | the window switcher for the windows of the current application |
| Alt+Space | the [window menu](#the-window-menu) of the active window |
| Alt+F4 | close the window |
| Super + L | lock the screen, see [Screen lock](lock.md) |
| F11 | fullscreen |
| Super + mouse | move a window |

The shortcuts of the shell follow a change of the keyboard layout. Alt+; is
taken only from the keys that type a semicolon without Shift or AltGr (in every
group of the layout), so Alt with the other keys stays for the applications.

## The window switcher

Alt+Tab shows, while Alt is held, a panel in the middle of the screen with an
icon and the name of every running application (once, with a dot for every
window when it has more). The window is switched when Alt is released, so the
cursor can also stay where it was. The applications are ordered by the use of
their windows (the most recently used first), so Alt+Tab alone goes to the
window used before.

Tab and Shift+Tab or the left and right arrows move the cursor, the down arrow
shows the windows of the application with big icons and their titles (the left
and right arrows choose one, Tab still moves between the applications) and the
up arrow goes back; Escape cancels.

## The launcher and the run dialog

Super opens the launcher, a full screen grid of the applications of the system
with a search field (where it looks for them is in
[Configuration](configuration.md#where-the-launcher-looks-for-applications)).
The right button on an application opens its menu, so it can be put in the dock
without starting it. Alt+F2 opens the run dialog. In their text field the arrows,
Home, End, Delete and a click move the cursor; Ctrl+V, Shift+Insert and the middle
button paste, Ctrl+C copies the text.

## The main menu

The logo at the left of the panel opens a menu with these items, in groups
(without the installed icons the button is the text Main Menu):

- **About Tiny Shell** starts `tiny-about`, a window with the logo, the version,
  the name of the system (`PRETTY_NAME` of `/etc/os-release`, or the name of
  `uname`), the kernel with the kind of the processor, the license and the
  address of the project. It is a window of its own, so the dock shows the logo
  for it. Escape, Return, Ctrl+Q and the button Close end it.
- **Settings** starts `tiny-settings` (the one next to `tiny-shell` when it was
  started with a path, else the one in `PATH`).
- **Applications** opens the launcher; **Terminal** starts the terminal of
  `[commands] terminal` (without the `-e` at its end).
- **Force Quit** starts `xkill`: click a window to end its program.
- **Restart** and **Shutdown** run `[commands] reboot` and `poweroff`.
- **Lock Screen** starts `tiny-lock` (or `[commands] lock`), see [Screen lock](lock.md).
- **Logout** ends tiny-shell.

A click on an item closes the menu. A program that cannot be started is reported
on stderr.

## The dock and the favorite applications

`[dock] mode` (or the setting Dock in `tiny-settings`) says when the dock is
shown. **autohide** (the default) hides it while the active window covers any
part of it, and brings it back when the pointer reaches the bottom edge of the
screen (a strip of a few pixels, scaled with `[theme] scale`); it hides again
when the pointer leaves it, but not while a menu of the dock is open or an icon
is dragged. **on** always shows it, **off** never does.

The dock has one icon for every application. All the windows of an application
(it is found by `WM_CLASS`; windows of an unknown application are
grouped by their class) share the icon, and a small dot under it stands for
every window; the dot of the window that has the focus is brighter. The favorite
applications come first, a thin line separates them from the other running
applications, and they stay in the dock when they do not run.

Click the icon of a favorite that does not run to start it. Click the icon of an
application that runs to focus its window; when it has more windows, every next
click goes to the next one. Drag the icon of a favorite to move it: a mark shows
where it will be put, the icon is drawn smaller meanwhile, and the new order is
saved; Escape cancels the drag and nothing moves.

Click the right button on an icon to open its menu:

- **New Window** starts the application again, also when it runs.
- The other actions of the desktop file (`[Desktop Action ...]`, for example
  "New Private Window").
- **All Windows ›** (when it runs) opens a submenu with the titles of its
  windows; a click focuses the window.
- **Add to Favorites** or **Remove from Favorites**.
- **Close Window**, or **Close All Windows** when there are more (when it runs),
  as if you clicked the close button of the windows.

The menu is as wide as its longest text. A click anywhere outside of the menu
closes it (the click does not reach the window below). In the launcher the right
button on an application opens the same menu, so you can put an application in
the dock without starting it. The favorites are saved at once to `[dock]
favorites`. An id of an application that is not installed is kept in the file
but not shown.

## The window menu

The menu has what a window can do and the title bar has no button for. It opens:

- with the right button anywhere on the title bar (at the pointer);
- with **Alt+Space** for the active window (below its title bar), or the action
  `window-menu`;
- when a program that draws its own title bar (GTK) asks for it with
  `_GTK_SHOW_WINDOW_MENU`, at the place it gives. The shell announces the
  request in `_NET_SUPPORTED`.

The items depend on the window, and what the window cannot do is not listed:

- **Maximize** or **Restore** (a maximized window has no Move, Resize and
  Roll Up);
- **Minimize**;
- **Move** and **Resize**: the window follows the pointer until you click (any
  of the three buttons), Escape puts it back. Moving does not need the pointer
  to go anywhere first, the window keeps its distance from it. Resizing moves
  the pointer to the bottom right corner of the window, which then follows it.
  Where the X server does not let a program move the pointer (Xephyr, a
  pointer that a virtual machine sets from outside), the size changes from
  where the pointer is, without a jump;
- **Roll Up** or **Roll Down**: the window is rolled up to its title bar (a
  double click on the title bar does the same);
- **Always on Top** or **Not Always on Top**;
- **Close**, last.

## Show desktop

The action `show-desktop` (or a request of a program or a pager,
`_NET_SHOWING_DESKTOP`) minimizes all the windows to show the desktop, and
asking again brings back the windows it hid. The windows that were minimized
before stay so. Focusing any window (Alt+Tab, the dock, a new window) ends the
mode and brings the others back. There is no key and no button for it yet.

## Startup notification

An application that tells when it has started (`StartupNotify=true` in its
desktop file) is started by the launcher or the dock with `DESKTOP_STARTUP_ID`
in its environment, and the pointer over the desktop is the "busy" one
(`XC_watch`) until its first window comes. The window ends the wait with
`_NET_STARTUP_ID`, or with the `WM_CLASS` that `StartupWMClass` names. The wait
also ends when the launch fails, and after 15 seconds, so a program that does
not know the protocol cannot keep the cursor busy. Other launchers can use the
[startup notification
protocol](https://specifications.freedesktop.org/startup-notification-spec/latest/)
too: tiny-shell watches the messages `_NET_STARTUP_INFO_BEGIN` and
`_NET_STARTUP_INFO` that they send to the root window. The dock does not mark
the starting application yet.
