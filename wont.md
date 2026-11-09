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
