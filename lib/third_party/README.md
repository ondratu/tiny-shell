# Vendored libraries

Copied unchanged from upstream; see CONTRIBUTING.md (Dependencies).

| File | Upstream | Version | Licence |
|------|----------|---------|---------|
| `nanosvg.h`, `nanosvgrast.h` | https://github.com/memononen/nanosvg | commit 239e102 (2026-07-10) | zlib, `LICENSE.nanosvg` |
| `stb_image.h` | https://github.com/nothings/stb | v2.30, commit 003501f (2026-10-22) | public domain / MIT, see the end of the file |

Used by `lib/image.cc` (the only file that defines the `*_IMPLEMENTATION`
macros): `stb_image` decodes PNG, `nanosvg` parses and rasterizes SVG.
