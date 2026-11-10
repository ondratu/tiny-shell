# Applications

The programs that come with tiny-shell.

tiny-shell comes with a few small programs of its own. They are built from the
same widget toolkit as the shell (see [For developers](developers/index.md)),
have a desktop file and an icon, so the launcher lists them and the dock shows
their icons.

| Program | What it is |
| ------- | ---------- |
| ![](static/icons/tiny-settings.svg) `tiny-settings` | the settings form, [below](#settings) |
| ![](static/icons/tiny-fonts.svg) `tiny-fonts` | the viewer of fonts, [below](#fonts) |
| ![](static/icons/tiny-shell.svg) `tiny-about` | a window with the version, the system and the license, started by the main menu |
| `tiny-wallpaper` | draws a picture as the background, see [Wallpaper](configuration.md#wallpaper) |
| `tiny-lock` | the screen lock, with `tiny-lock-auth` and `tiny-lock-pwcheck` that check the password, see [Screen lock](lock.md) |

## Settings

`build/tiny-settings` (made by `make`) has a panel at the left with the sections
and the fields of the chosen section. The sections are in the order of the
settings of GNOME:

- **Appearance:** the scale, the wallpaper and how it covers the screen (a small
  picture of the file above the path shows that the path is right).
- **Fonts:** the font (a family or a list of families separated by commas, like
  `DejaVu Sans,sans`), its size and style, and the rendering of text.
- **Desktop:** the mode of the dock (Auto-hide, On, Off) and the format of the
  clock.
- **Power:** Blank after, the time without input after which the
  [lock screen](lock.md) goes black: 1, 5, 10 or 15 minutes, or Never (`[lock]
  blank_after`). It is read the next time the screen is locked.
- **Applications:** the terminal.
- **Displays:** the primary and the secondary monitor (a choice of the monitors
  that the system has, like `1: DP-1 1920x1080`, or Auto), where the secondary
  one is (Keep, Right, Left, Above or Below the primary one). While this section
  is shown, the number of every monitor is on its screen, over all the windows,
  and Save applies the monitors at once.

A click on a section, or Ctrl+Page Down and Ctrl+Page Up, shows it. The form
shows the values in use now (the global file and the one of the user together).
A field has a cursor: the arrows, Home, End, Backspace and Delete work, and a
click puts the cursor where the pointer is. Ctrl+V, Shift+Insert and the middle
button paste the text of the clipboard or of the selection (one line of it),
Ctrl+C copies the text of the field. Tab moves between the fields of the
section. A field with a wrong value turns red and says what is wrong, and Save
waits until it is right (and shows the section where the wrong field is).

The button Choose... (or Down in the font field) shows a list of the fonts of
the system: type to search it, move with the arrows, Page Up and Page Down.
Under the list are the size and the style of the font, and a preview of a
sentence in the selected family, size and style, so you see how it will look
(the size follows `[theme] scale`, like in the shell). Tab moves between the
search, the size and the style; Up and Down change the size by a point in the
size field, and the arrows or a click change the style in the style field (it
offers the styles that the family has). Select (or Return) takes the three into
the form, Cancel (or Escape) goes back and changes nothing.

The rows Antialias, Hinting and Subpixel are choices, not text: the left and
right arrows, Space and a click go through the values, Backspace goes back to
Default. Default means that nothing is set and the programs keep what the system
(fontconfig) says. They set how the programs draw text, the way a GNOME settings
daemon does it for GTK, but through the X resources `Xft.antialias`,
`Xft.hinting`, `Xft.hintstyle` and `Xft.rgba`, which Xft (and so tiny-shell and
the programs that use Xft or read the resources, like Qt on X11) reads when a
program draws its first text. Save puts them into the `RESOURCE_MANAGER`
property of the root window at once (the other resources there stay), so the
programs that you start later use them; a program that runs keeps its look until
it is started again. tiny-shell also puts the values of the file there when it
starts, before it opens its first font. GTK programs read their own channels
(XSETTINGS, GSettings), which are not set by this.

The row Wallpaper is the path of a PNG or JPEG file, typed or pasted (there is
no file browser: it would be a program of its own, and Xlib has none). The row
Placement is a choice like the three above: Fill, Center or Tile. When one of
them changed, Save starts `tiny-wallpaper` at once, so you see the picture
without a new login. To remove a wallpaper, clear the row: the plain color of
the theme is back after the next login.

Save (or Return) writes the values you changed (`[theme] font`, `font_size`,
`font_style`, `[theme] scale`, `[commands] terminal`, `[xft] antialias`,
`hinting`, `rgba`, `[wallpaper] file`, `mode`, `[monitors]` ...) to the file of
the user, so the others still come from the global file, in the way that is
described in [Configuration](configuration.md) (the comments of the file are lost), and closes the program.
Cancel or Escape closes it and writes nothing. tiny-shell reads the file only
when it starts, so a change of the rest is seen after you log out and in again.

## Fonts

`build/tiny-fonts` (made by `make`) shows the font families of the system, the
list that `fc-list : family` prints, read from fontconfig. Every family is a
cell with "Aa" in the family (black on white) and its name under it. Type at the
top to search the families (Escape clears the search). Scroll with the wheel or
the keys, click a family (or press Return) to see its specimen: the small
letters, the capitals, the digits and the signs, then one sentence in 8, 12, 16,
20, 24, 32, 64 and 128 pixels. The button Info (or I) at the top right shows
what is known about the family: the styles, the format, the languages, the files
of its faces and, from the font file itself, the full name, the version, the
designer, the license and the copyright. The header has the breadcrumbs "‹ Fonts
‹ Family": a click on Fonts goes to the list, a click on the family back to the
specimen. Escape and backspace go one step back, Ctrl+Q and Ctrl+W quit. The
sizes follow `[theme] scale`.
