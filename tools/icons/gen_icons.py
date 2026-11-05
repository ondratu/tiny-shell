# tiny-shell symbolic icon set, 16x16 grid.
# Shapes are built from primitives with skia path ops and written out as
# plain filled paths (no strokes, masks or clip paths).
import os
import skia

GAP = 1.0  # the "seam" between titlebar and body, as in the logo


def rr(x, y, w, h, r):
    if not isinstance(r, tuple):
        r = (r, r, r, r)
    rect = skia.RRect()
    rect.setRectRadii(skia.Rect.MakeXYWH(x, y, w, h), [skia.Point(a, a) for a in r])
    p = skia.Path()
    p.addRRect(rect)
    return p


def circle(cx, cy, r):
    return skia.Path.Circle(cx, cy, r)


def poly(*pts, closed=False):
    p = skia.Path()
    p.moveTo(*pts[0])
    for pt in pts[1:]:
        p.lineTo(*pt)
    if closed:
        p.close()
    return p


def stroke(path, w=2.0, cap="round", join="round"):
    caps = {"round": skia.Paint.kRound_Cap, "butt": skia.Paint.kButt_Cap}
    joins = {"round": skia.Paint.kRound_Join, "miter": skia.Paint.kMiter_Join}
    paint = skia.Paint(Style=skia.Paint.kStroke_Style, StrokeWidth=w,
                       StrokeCap=caps[cap], StrokeJoin=joins[join])
    out = skia.Path()
    paint.getFillPath(path, out)
    return out


def union(*ps):
    out = ps[0]
    for p in ps[1:]:
        out = skia.Op(out, p, skia.PathOp.kUnion_PathOp)
    return out


def diff(a, *bs):
    for b in bs:
        a = skia.Op(a, b, skia.PathOp.kDifference_PathOp)
    return a


def grow(p, d=GAP):
    return union(p, stroke(p, 2 * d))


# ---- icons ---------------------------------------------------------------

def window_body(x, y, w, h, t=2.0, r=2.0):
    """U-shaped window body under a separate titlebar (the nut motif)."""
    return diff(rr(x, y, w, h, (0, 0, r, r)), skia.Path.Rect(skia.Rect.MakeXYWH(x + t, y - 1, w - 2 * t, h - t + 1)))


def icon_app():
    cap = rr(1, 2, 14, 3, (2, 2, 0, 0))
    return union(cap, window_body(1, 5 + GAP, 14, 8))


def icon_app_tile():
    """Generic application: rounded tile with an "A" cut out."""
    a = union(stroke(poly((4.6, 12.2), (8, 4.2), (11.4, 12.2)), 2.0, join="miter"),
              stroke(poly((6.2, 9.6), (9.8, 9.6)), 1.8, cap="butt"))
    return diff(rr(1, 1, 14, 14, 3.5), a)


def icon_font():
    """Font viewer: "Aa" standing on a baseline separated by the seam."""
    clip = skia.Path.Rect(skia.Rect.MakeXYWH(0, 0, 16, 12))
    a_big = stroke(poly((1.3, 12.8), (4.8, 3), (8.3, 12.8)), 2.0, cap="butt", join="miter")
    a_big = skia.Op(a_big, clip, skia.PathOp.kIntersect_PathOp)
    bar = stroke(poly((2.6, 8.8), (7, 8.8)), 1.6, cap="butt")
    bowl = stroke(circle(12.8, 9.4, 1.85), 1.5)
    stem = stroke(poly((14.6, 6.8), (14.6, 12)), 1.5, cap="butt")
    base = rr(1, 12 + GAP, 14, 2, 1)
    return union(a_big, bar, bowl, stem, base)


def icon_logo():
    cap = skia.Path()
    cap.moveTo(5, 2); cap.lineTo(11, 2); cap.cubicTo(13.4, 2, 14.9, 3, 14.9, 5)
    cap.lineTo(1.1, 5); cap.cubicTo(1.1, 3, 2.6, 2, 5, 2); cap.close()
    body = skia.Path()
    body.moveTo(1.1, 6); body.lineTo(14.9, 6); body.cubicTo(14.9, 10, 12.6, 13.6, 9, 14.8)
    body.quadTo(8, 15.5, 7, 14.8); body.cubicTo(3.4, 13.6, 1.1, 10, 1.1, 6); body.close()
    return union(cap, body)


def frame(x, y, w, h, top=3.0, side=2.0, r=1.5):
    return diff(rr(x, y, w, h, r), rr(x + side, y + top, w - 2 * side, h - top - side, 0.5))


def icon_close():
    return union(stroke(poly((4, 4), (12, 12))), stroke(poly((12, 4), (4, 12))))


def icon_minimize():
    return rr(3, 11, 10, 2, 1)


def icon_maximize():
    return frame(2.5, 2.5, 11, 11)


def icon_restore():
    front = frame(2, 6, 8, 8)
    back = diff(frame(6, 2, 8, 8), grow(rr(2, 6, 8, 8, 1.5)))
    return union(front, back)


def icon_shade():
    return union(rr(1, 4, 14, 4, 2), rr(3, 9 + GAP, 10, 2, 1), rr(5.5, 13, 5, 2, 1))


def icon_fullscreen():
    c = [poly((2, 6), (2, 2), (6, 2)), poly((10, 2), (14, 2), (14, 6)),
         poly((14, 10), (14, 14), (10, 14)), poly((6, 14), (2, 14), (2, 10))]
    return union(*[stroke(p) for p in c])


def icon_switch():
    front = rr(1, 5, 10, 9, 2)
    back = diff(frame(5, 2, 10, 9, top=2.5, r=2), grow(front))
    return union(front, back)


def icon_workspaces():
    return union(rr(1, 3, 6.5, 10, 1.5), frame(8.5, 3, 6.5, 10, top=2.5))


def icon_desktop():
    return union(rr(1, 2, 14, 9, 2), rr(4.5, 11 + 1.5, 7, 2, 1))


def icon_settings():
    lines = union(*[stroke(poly((1.5, y), (14.5, y))) for y in (4, 8, 12)])
    knobs = union(circle(5, 4, 2.25), circle(11, 8, 2.25), circle(7, 12, 2.25))
    return union(diff(lines, grow(knobs)), knobs)


def icon_help():
    q = skia.Path()
    q.moveTo(5.9, 6.1); q.cubicTo(5.9, 4.8, 6.8, 4, 8, 4)
    q.cubicTo(9.3, 4, 10.2, 4.9, 10.2, 6); q.cubicTo(10.2, 7.5, 8, 7.6, 8, 9.3)
    return diff(circle(8, 8, 7), stroke(q, 1.8), circle(8, 11.9, 1.1))


def icon_info():
    return diff(circle(8, 8, 7), circle(8, 4.8, 1.1), rr(7.1, 7, 1.8, 5.2, 0.9))


def icon_warning():
    tri = poly((8, 2.5), (14, 13.5), (2, 13.5), closed=True)
    shape = union(tri, stroke(tri, 2))
    return diff(shape, rr(7.1, 5.6, 1.8, 4.6, 0.9), circle(8, 11.9, 1.0))


def icon_error():
    x = union(stroke(poly((5.5, 5.5), (10.5, 10.5)), 1.8), stroke(poly((10.5, 5.5), (5.5, 10.5)), 1.8))
    return diff(circle(8, 8, 7), x)


def icon_user():
    body = skia.Path()
    body.moveTo(1.5, 15); body.cubicTo(1.5, 11.4, 4.4, 9, 8, 9)
    body.cubicTo(11.6, 9, 14.5, 11.4, 14.5, 15); body.close()
    return union(circle(8, 4.6, 3.2), body)


def icon_logout():
    br = skia.Path()
    br.moveTo(7.5, 2); br.lineTo(4, 2); br.quadTo(2, 2, 2, 4); br.lineTo(2, 12)
    br.quadTo(2, 14, 4, 14); br.lineTo(7.5, 14)
    arrow = union(stroke(poly((6.5, 8), (14, 8))), stroke(poly((11, 5), (14, 8), (11, 11))))
    return union(stroke(br), arrow)


def icon_lock():
    sh = skia.Path()
    sh.moveTo(5, 7 - GAP); sh.lineTo(5, 4.8)
    sh.arcTo(skia.Rect.MakeXYWH(5, 1.8, 6, 6), 180, 180, False)
    sh.lineTo(11, 7 - GAP)
    body = diff(rr(2, 7, 12, 8, 2), circle(8, 10.3, 1.3), rr(7.4, 10.3, 1.2, 2.7, 0.5))
    return union(body, stroke(sh, 2, cap="butt"))


def icon_shutdown():
    arc = skia.Path()
    arc.addArc(skia.Rect.MakeXYWH(2.5, 3.3, 11, 11), 305, 290)
    return union(stroke(arc), stroke(poly((8, 1.5), (8, 7.5))))


def icon_terminal():
    chev = stroke(poly((4.5, 5.5), (7, 8), (4.5, 10.5)), 1.8)
    return diff(rr(1, 2, 14, 12, 2.5), chev, rr(8.5, 9.6, 3.5, 1.8, 0.6))


def icon_app_grid():
    return union(*[circle(x, y, 1.5) for x in (3, 8, 13) for y in (3, 8, 13)])


def icon_search():
    ring = diff(circle(6, 6, 4.75), circle(6, 6, 2.75))
    d = 4.75 + GAP + 1.25
    s = 6 + d / 2 ** 0.5
    return union(ring, stroke(poly((s, s), (14, 14)), 2.5))


def icon_clock():
    return diff(circle(8, 8, 7), stroke(poly((8, 4), (8, 8), (10.8, 9.6)), 1.75))


def icon_menu():
    return union(rr(2, 3, 12, 2, 1), rr(2, 7, 12, 2, 1), rr(2, 11, 12, 2, 1))


def icon_pan_down():
    return stroke(poly((4, 6), (8, 10), (12, 6)))


def icon_pan_start():
    return stroke(poly((10, 4), (6, 8), (10, 12)))


def icon_pan_end():
    return stroke(poly((6, 4), (10, 8), (6, 12)))


# freedesktop.org icon naming where a standard name exists
ICONS = [
    ("start-here", icon_logo, "logo / menu"),
    ("application-x-executable", icon_app_tile, "aplikace bez ikony"),
    ("window", icon_app, "okno *"),
    ("preferences-desktop-font", icon_font, "písma"),
    ("window-close", icon_close, "zavřít"),
    ("window-minimize", icon_minimize, "minimalizovat"),
    ("window-maximize", icon_maximize, "maximalizovat"),
    ("window-restore", icon_restore, "obnovit"),
    ("window-shade", icon_shade, "sbalit do lišty *"),
    ("view-fullscreen", icon_fullscreen, "celá obrazovka"),
    ("window-switch", icon_switch, "přepnout okno *"),
    ("workspace-switcher", icon_workspaces, "plochy *"),
    ("user-desktop", icon_desktop, "zobrazit plochu"),
    ("view-app-grid", icon_app_grid, "aplikace"),
    ("system-run", icon_terminal, "spustit"),
    ("system-search", icon_search, "hledat"),
    ("preferences-system", icon_settings, "nastavení"),
    ("help-browser", icon_help, "nápověda"),
    ("avatar-default", icon_user, "uživatel"),
    ("system-lock-screen", icon_lock, "zamknout"),
    ("system-log-out", icon_logout, "odhlásit"),
    ("system-shutdown", icon_shutdown, "vypnout"),
    ("preferences-system-time", icon_clock, "hodiny"),
    ("open-menu", icon_menu, "menu"),
    ("dialog-information", icon_info, "informace"),
    ("dialog-warning", icon_warning, "varování"),
    ("dialog-error", icon_error, "chyba"),
    ("pan-down", icon_pan_down, "rozbalit"),
    ("pan-start", icon_pan_start, "zpět"),
    ("pan-end", icon_pan_end, "podmenu / další"),
]


def fmt(v):
    s = f"{v:.3f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def to_svg_d(path):
    out = []
    it = skia.Path.Iter(path, False)
    while True:
        verb, pts = it.next()
        V = skia.Path.Verb
        if verb == V.kDone_Verb:
            break
        if verb == V.kMove_Verb:
            out.append(f"M{fmt(pts[0].x())} {fmt(pts[0].y())}")
        elif verb == V.kLine_Verb:
            out.append(f"L{fmt(pts[1].x())} {fmt(pts[1].y())}")
        elif verb == V.kQuad_Verb:
            out.append("Q" + " ".join(f"{fmt(p.x())} {fmt(p.y())}" for p in pts[1:]))
        elif verb == V.kConic_Verb:
            w = it.conicWeight()
            p0, p1, p2 = pts
            k = 4 * w / (3 * (1 + w))
            c1 = (p0.x() + k * (p1.x() - p0.x()), p0.y() + k * (p1.y() - p0.y()))
            c2 = (p2.x() + k * (p1.x() - p2.x()), p2.y() + k * (p1.y() - p2.y()))
            out.append(f"C{fmt(c1[0])} {fmt(c1[1])} {fmt(c2[0])} {fmt(c2[1])} {fmt(p2.x())} {fmt(p2.y())}")
        elif verb == V.kCubic_Verb:
            out.append("C" + " ".join(f"{fmt(p.x())} {fmt(p.y())}" for p in pts[1:]))
        elif verb == V.kClose_Verb:
            out.append("Z")
    return "".join(out)


def write(dirname, color):
    os.makedirs(dirname, exist_ok=True)
    for name, fn, _ in ICONS:
        p = skia.Simplify(fn())
        rule = "evenodd" if p.getFillType() == skia.PathFillType.kEvenOdd else "nonzero"
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">\n'
               f'<path fill="{color}" fill-rule="{rule}" d="{to_svg_d(p)}"/>\n</svg>\n')
        open(os.path.join(dirname, f"{name}-symbolic.svg"), "w").write(svg)


if __name__ == "__main__":
    write("tiny-shell-icons/black", "#000000")
    write("tiny-shell-icons/white", "#ffffff")
    import json
    json.dump([(n, l) for n, _, l in ICONS], open("labels.json", "w"), ensure_ascii=False)
    print(len(ICONS), "icons")
