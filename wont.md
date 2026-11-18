# Won't do (for now)

What was judged and left out on purpose, with the reason and the condition
for coming back. Not to be mixed with todo.txt (the work) and backlog.md
(ideas that were not judged yet).

## `_NET_WM_BYPASS_COMPOSITOR` and `_NET_WM_OPAQUE_REGION`

- **Why not:** they are hints from a client to a compositor. tiny-shell is not
  one (see the README), and a compositor that runs next to it reads the
  properties from the client windows itself. There is nothing for the window
  manager to do, and listing them in `_NET_SUPPORTED` would promise
  something it does not do.
- **Back when:** tiny-shell gets a compositor of its own.

## Live thumbnails of windows

- **Why not:** a picture of a window (in Alt+Tab, in the dock or in an overview)
  needs the contents of windows that are covered or minimized. That is a
  compositor (offscreen pixmaps of every window, `XComposite`) or a copy of the
  windows by hand, and either costs memory and time on the old and small
  hardware that tiny-shell aims at. The switcher and the dock show the icon
  and the title.
- **Back when:** tiny-shell gets a compositor of its own, or an optional build
  feature that is off by default and costs nothing when it is off.

## The frame counter of `_NET_WM_SYNC_REQUEST` (the extended protocol)

- **Why not:** the second counter of `_NET_WM_SYNC_REQUEST_COUNTER` and the
  `_NET_WM_FRAME_DRAWN` / `_NET_WM_FRAME_TIMINGS` messages tell a compositor
  when a frame of the client was really drawn on the screen. tiny-shell is
  not a compositor (see the README); the basic counter is enough to resize at
  the speed of the client, and the code for the second one would never run.
- **Back when:** tiny-shell gets a compositor of its own.
