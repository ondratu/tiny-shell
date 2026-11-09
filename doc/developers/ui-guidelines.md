# User interface guidelines



tiny-shell does not copy the look of GNOME, but its
[Human Interface Guidelines](https://developer.gnome.org/hig/) are the
reference when we are not sure how something should behave. Take from them:

- **Room to breathe.** Controls are not squeezed together. An item of a menu
  is as high as the panel (`theme.wm_popup_item_height`), the text has a margin
  at both sides (`theme.wm_menu_padding`), a popup is as wide as its longest
  text and the margin, not a fixed width. Every size comes from the theme and
  scales with it.
- **Labels.** A command is a verb ("New Window", "Close Window"). Menu items
  and buttons use header capitalization ("Add to Favorites"), an ellipsis
  ("…") is only for an item that asks for more input before it acts. Menu text
  is aligned to the left.
- **Menus are short and ordered.** The most important item is first, the one
  that ends something (Close) is last. Related items are together. Do not nest
  submenus deeper than one level and keep them short. A context menu is shown
  only when it has something to offer.
- **Do not rely on color alone** to tell a state: the focused window has a
  brighter dot and a lighter background, a window that wants attention has a
  bar.
- **Pointer behavior.** Everything that the pointer does must be undoable or
  harmless: a click outside of a popup only closes it, a drag shorter than a
  few pixels is a click, a dragged thing shows where it will land and what it
  leaves behind. Do not use the middle button, double clicks or chords for
  anything essential. A pointer action should also be possible from the
  keyboard (not everything is yet).
- **Quiet defaults.** No animation, no sounds, no popup that the user did not
  ask for.

The shell is a dock, a panel and a launcher, so where GNOME has an idea for
them (the dash of GNOME Shell: running applications with a dot per window,
a line between favorites and the others, drag to reorder), follow it unless it
costs a lot of code or memory (see the [goals](index.md#goals-that-decide-everything-else)).
