# tiny-shell symbolic icons

30 symbolic icons on a 16x16 grid, in the shape language of the hazelnut
logo: solid fills, 2 px weight, rounded corners and a 1 px seam that
separates a "titlebar" from a "body".

- `black/` and `white/` (in `data/tiny-shell/icons/`) - the same icons, one filled path each, no strokes,
  masks, clip paths or gradients (safe for nanosvg, librsvg and friends)
- `gen_icons.py` (in `tools/icons/`; it writes into `data/tiny-shell/icons/`) - the generator (needs `pip install skia-python`); edit
  the shapes there and regenerate rather than editing the SVG files

Names follow the freedesktop.org Icon Naming Specification with the
`-symbolic` suffix. Four names are not in the specification and are
specific to tiny-shell: `window`, `window-shade`, `window-switch`,
`workspace-switcher`.
