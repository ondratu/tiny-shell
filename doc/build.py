#!/usr/bin/env python3
"""Build the web site of tiny-shell from the Markdown files in doc/.

    pip install -r doc/requirements.txt
    python3 doc/build.py            # writes build-site/
    python3 doc/build.py -o /tmp/s  # somewhere else

The pages are converted from Markdown to reStructuredText by m2r2 and from
that to HTML by docutils, then put into one template (doc/static/style.css
styles it). The icons of the site are the ones of the shell, from data/.
The build fails when a link between the pages points nowhere, so the CI
catches it.
"""

import argparse
import html
import posixpath
import re
import shutil
import sys
from pathlib import Path

import docutils.core
import m2r2

DOC = Path(__file__).resolve().parent
ROOT = DOC.parent
REPO = "https://github.com/ondratu/tiny-shell"

# The menu is a tree: a page is (file, label), a group is (label, [pages]).
# A group is a drop-down of the top menu and a list at the side of its pages.
# The first page of a group with an own start page is the start page.
NAV = [
    ("index.md", "Home"),
    ("Guide", [
        ("install.md", "Installation"),
        ("usage.md", "Using the shell"),
        ("configuration.md", "Configuration"),
        ("applications.md", "Applications"),
    ]),
    ("Project", [
        ("features.md", "Features"),
        ("comparison.md", "Comparison"),
        ("screenshots.md", "Screenshots"),
        ("roadmap.md", "Roadmap"),
    ]),
    ("download.md", "Download"),
    ("Developers", [
        ("developers/index.md", "Overview"),
        ("developers/conventions.md", "Conventions"),
        ("developers/code-layout.md", "Layout of the sources"),
        ("developers/architecture.md", "Architecture"),
        ("developers/dependencies.md", "Dependencies"),
        ("developers/ui-guidelines.md", "User interface"),
        ("developers/testing.md", "Setup and tests"),
    ]),
    ("more.md", "More"),
]


def pages():
    """All the pages of the menu in order, as (file, label)."""
    for item in NAV:
        if isinstance(item[1], list):
            yield from item[1]
        else:
            yield item


def group_of(page: str):
    for item in NAV:
        if isinstance(item[1], list) and any(src == page for src, _ in item[1]):
            return item
    return None


# Cards of the home page: (icon of data/tiny-shell/icons/black, title, text, page)
CARDS = [
    ("window", "Window manager",
     "Reparenting frames, a window menu, a window switcher. ICCCM and EWMH, so "
     "that ordinary programs just work.", "features.md"),
    ("view-app-grid", "Panel, dock and launcher",
     "A panel with a menu and a clock, a dock with favorites and a full screen "
     "launcher with search.", "usage.md"),
    ("preferences-system", "Settings",
     "A small form for fonts, the scale, the wallpaper and the monitors, and "
     "the XSETTINGS of GTK and Qt.", "applications.md"),
    ("preferences-desktop-font", "Tiny",
     "A few thousand lines of C++ on top of Xlib. No GLib, no D-Bus, no "
     "compositor.", "developers/index.md"),
]

SYMBOLS = {"✓": ("yes", "Yes"), "✗": ("no", "No"), "◔": ("plan", "Planned"),
           "?": ("maybe", "Undecided")}


def title_of(md: str) -> str:
    m = re.search(r"^# (.+)$", md, re.M)
    return m.group(1).strip() if m else "tiny-shell"


def to_html(md: str) -> str:
    rst = m2r2.convert(md)
    parts = docutils.core.publish_parts(
        rst,
        writer_name="html5",
        settings_overrides={
            "initial_header_level": 1,
            "doctitle_xform": False,
            "syntax_highlight": "short",
            "report_level": 2,
            "halt_level": 2,
            "embed_stylesheet": False,
            "stylesheet_path": None,
        },
    )
    return parts["body"]


def post(body: str) -> str:
    # links to the other pages
    body = re.sub(r'href="([^":#]+)\.md(#[^"]*)?"', r'href="\1.html\2"', body)
    # a symbol of the comparison is a colored cell
    def cell(m):
        css, word = SYMBOLS[m.group(1)]
        return ('<td class="%s"><p><span aria-hidden="true">%s</span>'
                '<span class="sr-only">%s</span></p></td>' % (css, m.group(1), word))
    body = re.sub(r"<td><p>([✓✗◔?])</p></td>", cell, body)
    # wide tables scroll inside their box, not the page
    body = re.sub(r"(<table\b.*?</table>)", r'<div class="table">\1</div>', body, flags=re.S)
    return body


def rel(page: str) -> str:
    """The path from a page to the root of the site."""
    return "../" * page.count("/")


def link(page: str, src: str, label: str) -> str:
    cur = ' class="current" aria-current="page"' if page == src else ""
    return '<a href="%s%s"%s>%s</a>' % (rel(page), src[:-3] + ".html", cur, label)


def menu(page: str) -> str:
    """The top menu as nested lists: the CSS makes drop-downs of them, and the
    script folds the whole menu into a button when it does not fit."""
    here = group_of(page)
    items = []
    for item in NAV:
        if isinstance(item[1], list):
            label, children = item
            lis = "".join("<li>%s</li>" % link(page, s, l) for s, l in children)
            cur = ' class="group current"' if item is here else ' class="group"'
            items.append(
                '<li%s><button type="button" aria-haspopup="true" aria-expanded="false">%s</button>'
                "<ul>%s</ul></li>" % (cur, label, lis))
        else:
            items.append('<li class="%s">%s</li>' % (
                "current" if item[0] == page else "single", link(page, *item)))
    return "<ul>%s</ul>" % "\n".join(items)


def side(page: str) -> str:
    group = group_of(page)
    if group is None:
        return ""
    items = [link(page, src, label) for src, label in group[1]]
    return '<nav class="side" aria-label="%s">\n%s\n</nav>' % (group[0], "\n".join(items))


def hero(page: str) -> str:
    if page != "index.md":
        return ""
    cards = []
    for icon, title, text, target in CARDS:
        cards.append(
            '<a class="card" href="%s"><img src="static/icons/%s-symbolic.svg" alt="" '
            'width="32" height="32"><h3>%s</h3><p>%s</p></a>'
            % (target[:-3] + ".html", icon, title, text))
    return '<div class="cards">\n%s\n</div>' % "\n".join(cards)


TEMPLATE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="description" content="A very small and light window manager and desktop shell for X11, written directly on top of Xlib.">
<link rel="icon" type="image/svg+xml" href="{base}static/icons/tiny-shell.svg">
<link rel="stylesheet" href="{base}static/style.css">
<script>document.documentElement.className = "js";</script>
</head>
<body>
<header class="top">
<div class="bar">
<a class="brand" href="{base}index.html"><img src="{base}static/icons/tiny-shell.svg" alt="" width="32" height="32">tiny-shell</a>
<button class="burger" type="button" aria-controls="menu" aria-expanded="false">Menu</button>
<nav class="menu" id="menu" aria-label="Main">
{menu}
</nav>
<a class="gh" href="{repo}">GitHub</a>
</div>
</header>
<div class="page{wide}">
{side}
<main>
{body}
</main>
</div>
<footer>
<p>tiny-shell is released under the <a href="{repo}/blob/master/LICENSE">BSD 2-Clause License</a>.
<a href="{repo}/blob/master/doc/{src}">Edit this page</a>.</p>
</footer>
<script src="{base}static/menu.js" defer></script>
</body>
</html>
"""


def build(out: Path) -> int:
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    (out / "developers").mkdir()

    sources = sorted(p.relative_to(DOC).as_posix() for p in DOC.rglob("*.md"))
    listed = {s for s, _ in pages()}
    missing = [s for s in sources if s not in listed]
    if missing:
        print("not in the menu (NAV) of build.py:", ", ".join(missing), file=sys.stderr)
        return 1

    ids: dict[str, set[str]] = {}
    links: list[tuple[str, str]] = []
    for src in sources:
        md = (DOC / src).read_text(encoding="utf-8")
        page = src[:-3] + ".html"
        body = post(to_html(md))
        if src == "index.md":
            at = body.index('<section id="what-it-is">')
            body = body[:at] + hero(src) + "\n" + body[at:]
        ids[page] = set(re.findall(r'\bid="([^"]+)"', body))
        for href in re.findall(r'href="([^"]+)"', body):
            if not re.match(r"[a-z]+:", href):
                links.append((page, href))
        for img in re.findall(r'src="([^"]+)"', body):
            if not re.match(r"[a-z]+:", img):
                links.append((page, img))
        text = TEMPLATE.format(
            title=html.escape(title_of(md) if src != "index.md" else "tiny-shell")
            + ("" if src == "index.md" else " - tiny-shell"),
            base=rel(src), menu=menu(src), side=side(src), body=body,
            repo=REPO, src=src,
            wide=" wide" if src == "comparison.md" else "",
        )
        (out / page).write_text(text, encoding="utf-8")

    shutil.copytree(DOC / "static", out / "static")
    icons = out / "static" / "icons"
    icons.mkdir(exist_ok=True)
    for card in CARDS:
        shutil.copy(ROOT / "data/tiny-shell/icons/black" / ("%s-symbolic.svg" % card[0]), icons)

    bad = 0
    for page, href in links:
        target, _, frag = href.partition("#")
        dest = posixpath.normpath(posixpath.join(posixpath.dirname(page), target)) if target else page
        if target and not (out / dest).exists():
            print("%s: no file for %s" % (page, href), file=sys.stderr)
            bad += 1
        elif frag and dest.endswith(".html") and frag not in ids.get(dest, set()):
            print("%s: no anchor %s" % (page, href), file=sys.stderr)
            bad += 1
    print("%d pages in %s" % (len(sources), out))
    return 1 if bad else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("-o", "--output", type=Path, default=ROOT / "build-site")
    return build(ap.parse_args().output)


if __name__ == "__main__":
    sys.exit(main())
