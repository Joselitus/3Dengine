"""Generates rv.obj / rv.mtl: a retro RV, wheel_negx.obj / wheel_posx.obj, its wheels, and headlight_glow.obj, the lit lenses of its headlights (drawn only while they are on). Y up, front of the vehicle toward +Z, units in meters."""
import math

W = 1.2  # half width
wheels = {}       # side (-1 / +1) -> [(material, verts, faces)] of one wheel
wheel_parts = []  # indexes in objs of every tire, hub and cap
objs = []  # (material, verts, faces)

def cur(mat):
    objs.append((mat, [], []))
    return objs[-1]

def vol(verts, faces):
    s = 0
    for f in faces:
        a = verts[f[0]]
        for i in range(1, len(f) - 1):
            b, c = verts[f[i]], verts[f[i + 1]]
            s += (a[0]*(b[1]*c[2]-b[2]*c[1]) - a[1]*(b[0]*c[2]-b[2]*c[0]) + a[2]*(b[0]*c[1]-b[1]*c[0]))
    return s

def add(mat, verts, faces):
    if vol(verts, faces) < 0:
        faces = [f[::-1] for f in faces]
    objs.append((mat, verts, faces))

def hexa(mat, v):
    """v: 4 verts of one face (ccw-ish loop) then the 4 matching verts of the opposite face."""
    f = [[0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]]
    add(mat, list(v), f)

def box(mat, x0, x1, y0, y1, z0, z1):
    hexa(mat, [(x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1),
               (x0,y1,z0),(x1,y1,z0),(x1,y1,z1),(x0,y1,z1)])

MIRROR_TURN = {1: (14.0, 4.8), -1: (22.5, 3.7)}  # side -> (yaw, pitch) in degrees: the driver's (+x) and passenger's (-x) heads, each
# aimed so that the driver's line of sight bounces straight back (yaw is mirrored for the -x one: see turned_box)

def turned_box(mat, centre, half, yaw, pitch, side=1):
    """A box turned about its own centre: pitch about its x axis (positive tilts its rear face up),
    then yaw about the vertical (positive turns the rear face towards -x, as seen on the +x side;
    mirrored for side = -1)."""
    ya, pa = math.radians(yaw) * side, math.radians(pitch)
    cy, sy, cp, sp = math.cos(ya), math.sin(ya), math.cos(pa), math.sin(pa)
    def turn(v):
        x, y, z = v
        y, z = y*cp - z*sp, y*sp + z*cp   # about x
        x, z = x*cy + z*sy, -x*sy + z*cy  # about y
        return (centre[0] + x, centre[1] + y, centre[2] + z)
    hx, hy, hz = half
    c = [(-hx,-hy,-hz),(hx,-hy,-hz),(hx,-hy,hz),(-hx,-hy,hz),(-hx,hy,-hz),(hx,hy,-hz),(hx,hy,hz),(-hx,hy,hz)]
    hexa(mat, [turn(v) for v in c])

def prism(mat, prof, x0, x1):
    n = len(prof)
    verts = [(x0, y, z) for z, y in prof] + [(x1, y, z) for z, y in prof]
    faces = [list(range(n)), list(range(2*n-1, n-1, -1))]
    for i in range(n):
        j = (i + 1) % n
        faces.append([i, n+i, n+j, j])  # same turn as the caps (outward together)
    add(mat, verts, faces)

def cyl(mat, cx, cy, cz, r, x0, x1, seg=24, ymin=None):
    verts, faces = [], []
    for x in (x0, x1):
        for i in range(seg):
            a = 2*math.pi*i/seg
            y = cy + r*math.sin(a)
            verts.append((x, y if ymin is None else max(y, ymin), cz + r*math.cos(a)))
    faces.append(list(range(seg)))
    faces.append(list(range(2*seg-1, seg-1, -1)))
    for i in range(seg):
        j = (i+1) % seg
        faces.append([i, seg+i, seg+j, j])  # same turn as the caps (outward together)
    add(mat, verts, faces)

# body profile (z, y): rear -> front, slanted windshield, nearly flat roof
# Things stuck on the body start GAP outside its walls: flush with them they would
# share a plane with the inside of the wall, and the cab (the camera sits in it)
# would flicker where they meet.
GAP = 0.004
PROF = [(-3.55,0.55),(3.55,0.55),(3.55,1.7),(3.0,2.95),(2.85,3.05),(-3.4,3.05),(-3.55,2.9)]
# The body is the prism of PROF (see prism()) but with openings cut out of it:
# - the two windshield panes, in its slanted front face: along that face (edge 2
#   of PROF, from A to B) the strip T0..T1 has no wall between the panes' x
#   ranges, apart from the post in the middle;
# - one window on each side of the cab, beside the driver: a rectangle of the
#   side walls (SIDE_WINDOW = z0, z1, y0, y1).
# Through them (and their translucent glass) the driver can see out and the
# camera can sit in the cab.
T0, T1 = 0.08, 0.85
PANES = ((-1.05, -0.03), (0.03, 1.05))
# The side window follows the slant of the windshield: its front edge is parallel
# to it (a thin pillar in between), so it reaches almost to the front; (z, y),
# counter-clockwise like PROF
SIDE_WINDOW = [(1.3, 1.85), (3.334, 1.85), (3.048, 2.5), (1.3, 2.5)]

def ray_hit(c, w, edge):
    """Where the ray from c through w meets the segment `edge`: (t along the edge, distance) or None."""
    (ax, ay), (bx, by) = edge
    dx, dy = w[0] - c[0], w[1] - c[1]
    ex, ey = bx - ax, by - ay
    den = dx * ey - dy * ex
    if abs(den) < 1e-12:
        return None
    s_ = ((ax - c[0]) * ey - (ay - c[1]) * ex) / den   # along the ray (1 = at w)
    t = ((ax - c[0]) * dy - (ay - c[1]) * dx) / den    # along the edge
    if s_ > 1.0 and -1e-9 <= t <= 1 + 1e-9:
        return t, s_
    return None

# The door: an opening in the +x wall (the driver's side), z from -0.8 to 0.1 and y from 0.6 to 2.45
# (counter-clockwise in (z, y), like PROF). RV.cpp's DOOR_* and the hull's gap must be the same.
DOOR_HOLE = [(-0.8, 0.6), (0.1, 0.6), (0.1, 2.45), (-0.8, 2.45)]

def _cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

def _same(a, b):
    return abs(a[0] - b[0]) < 1e-9 and abs(a[1] - b[1]) < 1e-9

def _crosses(p1, p2, p3, p4):
    """Do the segments p1-p2 and p3-p4 cross at a point inside both (not at a shared end)?"""
    if _same(p1, p3) or _same(p1, p4) or _same(p2, p3) or _same(p2, p4):
        return False
    d1, d2 = _cross(p3, p4, p1), _cross(p3, p4, p2)
    d3, d4 = _cross(p1, p2, p3), _cross(p1, p2, p4)
    return ((d1 > 1e-12 and d2 < -1e-12) or (d1 < -1e-12 and d2 > 1e-12)) and \
           ((d3 > 1e-12 and d4 < -1e-12) or (d3 < -1e-12 and d4 > 1e-12))

def _inside_polygon(p, poly):
    inside = False
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        if (a[1] > p[1]) != (b[1] > p[1]):
            x = a[0] + (p[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1])
            if p[0] < x:
                inside = not inside
    return inside

def triangulate(outer, holes):
    """Triangles (point triples, counter-clockwise) of the polygon `outer` (counter-clockwise, (z, y) points,
    possibly with collinear points on its sides, which are kept as vertices) with the `holes` cut out."""
    poly = list(outer)
    hole_loops = [list(reversed(h)) if _cross(h[0], h[1], h[2]) > 0 else list(h) for h in holes]
    all_hole_edges = [(h[i], h[(i + 1) % len(h)]) for h in hole_loops for i in range(len(h))]
    for h in sorted(hole_loops, key=lambda h: min(p[0] for p in h)):
        start = min(range(len(h)), key=lambda i: h[i][0])
        hv = h[start]
        order = sorted(range(len(poly)), key=lambda i: (poly[i][0] - hv[0]) ** 2 + (poly[i][1] - hv[1]) ** 2)
        for i in order:
            v = poly[i]
            mid = ((v[0] + hv[0]) / 2, (v[1] + hv[1]) / 2)
            edges = [(poly[k], poly[(k + 1) % len(poly)]) for k in range(len(poly))] + all_hole_edges
            if any(_crosses(hv, v, a, b) for a, b in edges):
                continue
            if not _inside_polygon(mid, poly) or any(_inside_polygon(mid, hh) for hh in hole_loops):
                continue
            loop = h[start:] + h[:start] + [hv]
            poly = poly[:i + 1] + loop + [v] + poly[i + 1:]
            break
        else:
            raise RuntimeError('no bridge to a hole')
    tris = []
    guard = 0
    while len(poly) > 3 and guard < 10000:
        guard += 1
        n = len(poly)
        for i in range(n):
            a, b, c = poly[i - 1], poly[i], poly[(i + 1) % n]
            cr = _cross(a, b, c)
            if abs(cr) < 1e-12:                      # a collinear point (or a spike): no area, drop it
                if not _same(a, c):
                    pass
                poly.pop(i)
                break
            if cr < 0:
                continue                             # a reflex corner
            if any(not (_same(p, a) or _same(p, b) or _same(p, c)) and
                   _cross(a, b, p) > 1e-12 and _cross(b, c, p) > 1e-12 and _cross(c, a, p) > 1e-12
                   for p in poly):
                continue                             # something is inside this ear
            tris.append((a, b, c))
            poly.pop(i)
            break
        else:
            raise RuntimeError('triangulation stuck')
    if len(poly) == 3 and abs(_cross(*poly)) > 1e-12:
        tris.append(tuple(poly))
    return tris

def body_with_openings():
    n = len(PROF)
    verts = []
    cache = {}
    def vert(x, z, y):
        k = (round(x, 5), round(z, 5), round(y, 5))
        if k not in cache:
            cache[k] = len(verts); verts.append((x, y, z))
        return cache[k]
    def pt(i, t):  # point of profile edge i -> i+1 at t
        (za, ya), (zb, yb) = PROF[i], PROF[(i + 1) % n]
        return (za + (zb - za) * t, ya + (yb - ya) * t)
    faces = []
    # The side walls (the caps of the prism) minus the window: the ring between
    # the window and the outline is cut into one piece per window corner, along
    # the rays from the window's centre through its corners. `splits[i]` keeps
    # the t where those rays meet profile edge i, so the quad of that edge can
    # be split there too (no vertex of a wall piece without one on the quad
    # next to it).
    wc = (sum(p[0] for p in SIDE_WINDOW) / len(SIDE_WINDOW), sum(p[1] for p in SIDE_WINDOW) / len(SIDE_WINDOW))
    splits = {i: set() for i in range(n)}
    hits = []
    for w in SIDE_WINDOW:
        best = None
        for e in range(n):
            h = ray_hit(wc, w, (PROF[e], PROF[(e + 1) % n]))
            if h and (best is None or h[1] < best[2]):
                best = (e, h[0], h[1])
        hits.append(best)
        splits[best[0]].add(best[1])
    # The outline with every split point in it (the rays' hits, and the
    # windshield's T0 and T1 on edge 2), in order, so that a wall piece has the
    # same vertices on its border as the quads beside it
    split_ts = {i: sorted({0.0} | splits[i] | ({T0, T1} if i == 2 else set())) for i in range(n)}
    ring, where = [], {}
    for i in range(n):
        for t in split_ts[i]:
            where[(i, round(t, 9))] = len(ring)
            ring.append(pt(i, t))
    m = len(SIDE_WINDOW)
    # the +x wall has the door opening as well as the window: it is triangulated from the outline
    # (with the same split points, so that it meets the quads beside it vertex to vertex)
    for a, b, c in triangulate(ring, [SIDE_WINDOW, DOOR_HOLE]):
        faces.append([vert(W, p[0], p[1]) for p in (c, b, a)])  # (the +x side faces the other way)
    for x, flip in ((-W, False),):
        for k in range(m):
            k2 = (k + 1) % m
            i1 = where[(hits[k][0], round(hits[k][1], 9))]
            i2 = where[(hits[k2][0], round(hits[k2][1], 9))]
            piece = [ring[i1]]
            i = i1
            while i != i2:                       # along the outline, counter-clockwise
                i = (i + 1) % len(ring)
                piece.append(ring[i])
            piece += [SIDE_WINDOW[k2], SIDE_WINDOW[k]]
            idx = [vert(x, z, y) for z, y in piece]
            faces.append(idx[::-1] if flip else idx)
    # The other faces of the prism, one quad per profile edge, split at those
    # points, and with the windshield openings
    for i in range(n):
        ts = {0.0, 1.0} | splits[i]
        if i == 2:
            ts |= {T0, T1}
        ts = sorted(ts)
        for ta, tb in zip(ts, ts[1:]):
            if tb - ta < 1e-9:
                continue
            def quad(x0, x1):  # same turn as prism(): [a at x0, a at x1, b at x1, b at x0]
                za_, ya_ = pt(i, ta); zb_, yb_ = pt(i, tb)
                faces.append([vert(x0, za_, ya_), vert(x1, za_, ya_), vert(x1, zb_, yb_), vert(x0, zb_, yb_)])
            # every strip is cut at the panes' x (the whole prism, so that the
            # strips above and below the openings meet the posts, and the
            # neighbouring edges' quads, vertex to vertex)
            middle = i == 2 and T0 - 1e-9 <= ta and tb <= T1 + 1e-9
            xs = [-W, PANES[0][0], PANES[0][1], PANES[1][0], PANES[1][1], W]
            for k in range(len(xs) - 1):
                if middle and (xs[k], xs[k+1]) in PANES:
                    continue                                       # the openings
                quad(xs[k], xs[k+1])
    objs.append(('body', verts, faces))
body_with_openings()

def lerp(a, b, t): return tuple(a[i]+(b[i]-a[i])*t for i in range(len(a)))

# slanted front face from (3.55,1.7) to (3.0,2.95)
A, B = (3.55, 1.7), (3.0, 2.95)
nz, ny = 1.25, 0.55
l = math.hypot(nz, ny); nz, ny = nz/l, ny/l
def front(t, x, off=0.0):
    z, y = lerp(A, B, t)
    return (x, y + ny*off, z + nz*off)
# (the windshield panes are models of their own, so that the broken one can replace them: see
# generate_windshield.py; keep PROF / T0 / T1 / PANES / A / B the same there)
# side windows of the cab: a thin pane in each opening of the walls
for sx in (-1, 1):
    prism('sideglass', SIDE_WINDOW, min(sx*W - 0.01, sx*W + 0.01), max(sx*W - 0.01, sx*W + 0.01))
# (the dashboard is a model of its own, dashboard.obj: see generate_dashboard.py)
# front lower fascia: grille + headlights on the vertical front (z=3.5..3.55)
FZ = 3.55
box('grille', -0.85, -0.05, 0.85, 1.2, FZ+GAP, FZ+0.04)
box('grille', 0.05, 0.85, 0.85, 1.2, FZ+GAP, FZ+0.04)
for sx in (-1, 1):
    for y in (0.9, 1.08):
        cyl_x = sx*1.0
        # headlight: cylinder with axis along z, built by rotating cyl coordinates
        seg = 16; r = 0.075; verts = []; faces = []
        for z in (FZ, FZ+0.06):
            for i in range(seg):
                a = 2*math.pi*i/seg
                verts.append((cyl_x + r*math.cos(a), y + r*math.sin(a), z))
        faces.append(list(range(seg))); faces.append(list(range(2*seg-1, seg-1, -1)))
        for i in range(seg):
            j = (i+1) % seg; faces.append([i, seg+i, seg+j, j])
        add('light', verts, faces)
# bumper
box('bumper', -1.25, 1.25, 0.5, 0.75, 3.35, 3.7)
box('bumper', -1.25, 1.25, 0.5, 0.75, -3.7, -3.4)
# side details. Stripes are 0.02 thick, door/windows sit above them so nothing is coplanar.
for sx in (-1, 1):
    def sbox(mat, y0, y1, z0, z1, d0=GAP, d1=0.02):
        a, b = sx*(W+d0), sx*(W+d1)
        box(mat, min(a, b), max(a, b), y0, y1, z0, z1)
    sbox('orange', 2.55, 2.75, -3.55, 3.08)
    # the two bands along the sides: on the +x side they stop at the doorway (z -0.82 to 0.12, where the
    # door covers it): they would stay floating in the air when the door is open
    for z0, z1 in (((-3.55, -0.82), (0.12, 3.55)) if sx > 0 else ((-3.55, 3.55),)):
        sbox('teal', 1.3, 1.4, z0, z1)
        sbox('orange', 1.2, 1.3, z0, z1)
    for z0, z1 in ((-3.1,-2.2),(-1.9,-1.0)):
        sbox('glass', 1.6, 2.35, z0, z1, GAP, 0.03)
    sbox('glass', 1.9, 2.5, 0.45, 0.9, GAP, 0.03)  # small cab-side window
    if sx > 0:
        # (the door only on the +X side: a model of its own, door.obj, see below)
        sbox('bumper', 0.28, 0.34, -0.75, 0.05, 0.0, 0.30)  # the step under it
        sbox('rack', 0.34, 0.60, -0.50, -0.46, 0.0, 0.02)   # ...and its bracket
    else:
        sbox('glass', 1.6, 2.35, -0.7, 0.0, GAP, 0.03)
    # mirror: arm + head
    sbox('mirror', 1.72, 1.80, 3.33, 3.43, 0.0, 0.25)  # (the arm joins the head from below, out of the glass's way)
    # The head is 0.22 wide, 0.42 tall and 0.12 deep, turned as a whole (MIRROR_YAW about the vertical,
    # then MIRROR_PITCH about its own x) so that the driver sees the road behind in it, and not the
    # RV's flank. RV.cpp puts a glass, flat on the rear face of each (MIRROR_* there).
    mx, my, mz = sx*(W+0.31), 2.01, 3.38
    turned_box('mirror', (mx, my, mz), (0.11, 0.21, 0.06), MIRROR_TURN[sx][0], MIRROR_TURN[sx][1], sx)
    # wheels: dark arch disc on the body side, tire straddling the wall, hub caps
    # (the tire, hub and cap are separate models, see below: they move with
    # the suspension)
    for cz in (2.4, -2.3):
        first = len(objs)
        lo, hi = (W+0.01, W+0.02)
        cyl('arch', 0, 0.5, cz, 0.62, sx*lo if sx > 0 else sx*hi, sx*hi if sx > 0 else sx*lo, 28, ymin=0.56)
        a, b = sx*(W-0.2), sx*(W+0.18)
        cyl('tire', 0, 0.5, cz, 0.5, min(a, b), max(a, b), 28)
        a, b = sx*(W+0.18), sx*(W+0.2)
        cyl('hub', 0, 0.5, cz, 0.28, min(a, b), max(a, b), 20)
        a, b = sx*(W+0.2), sx*(W+0.22)
        cyl('cap', 0, 0.5, cz, 0.09, min(a, b), max(a, b), 12)
        # the wheel that is written to its own file: the first one on each side
        # (the rest are the same shape), centred on its axle
        if cz == 2.4:
            wheels[sx] = [(m, [(x - sx*W, y - 0.5, z - cz) for x, y, z in v], f)
                          for m, v, f in objs[first+1:]]
        wheel_parts.extend(range(first + 1, len(objs)))
# rear: long window + ladder (stair) on the right
RZ = -3.55
box('glass', -1.0, 0.45, 1.6, 2.35, RZ-0.03, RZ-GAP)
box('orange', -W, W, 2.55, 2.75, RZ-0.02, RZ-GAP)
for x in (0.7, 1.0):
    box('rack', x-0.025, x+0.025, 0.85, 2.95, RZ-0.1, RZ-0.05)           # rails
    for y in (0.85, 2.9):
        box('rack', x-0.02, x+0.02, y, y+0.05, RZ-0.1, RZ-GAP)               # standoffs
for i in range(8):
    y = 1.0 + i*0.25
    box('rack', 0.7, 1.0, y, y+0.04, RZ-0.1, RZ-0.06)                    # rungs
# front orange stripe across the cab + teal
box('orange', -W, W, 1.2, 1.3, FZ+GAP, FZ+0.02)
box('teal', -W, W, 1.3, 1.4, FZ+GAP, FZ+0.02)
# roof rack (rear) and roof vent bar
for sx in (-1, 1):
    box('rack', sx*0.9-0.03, sx*0.9+0.03, 3.05+GAP, 3.3, -3.3, -3.24)
    box('rack', sx*0.9-0.03, sx*0.9+0.03, 3.05+GAP, 3.3, -0.5, -0.44)
box('rack', -0.93, 0.93, 3.27, 3.33, -3.3, -0.44)
box('rack', -0.45, 0.45, 3.05+GAP, 3.2, 0.3, 2.3)  # roof AC unit
MATS = {
 'body': (0.97, 0.92, 0.80), 'glass': (0.05, 0.30, 0.45),
 'orange': (1.00, 0.45, 0.05), 'teal': (0.05, 0.90, 0.65), 'grille': (0.20, 0.17, 0.12),
 'light': (1.00, 1.00, 1.00), 'bumper': (0.80, 0.55, 0.28), 'mirror': (0.60, 0.60, 0.62),
 'rack': (0.88, 0.84, 0.74), 'tire': (0.22, 0.20, 0.17), 'hub': (0.75, 0.62, 0.38),
 'door': (0.90, 0.84, 0.72), 'arch': (0.12, 0.11, 0.09), 'cap': (0.55, 0.45, 0.28),
 'glow': (1.00, 0.95, 0.70), 'sideglass': (0.55, 0.78, 0.88),
}
# materials that are translucent: opacity (1 = solid), written as `d` in the .mtl
OPACITY = {'sideglass': 0.15}
with open('rv.mtl', 'w') as f:
    f.write('# generated by generate_rv.py\n')
    for n, c in MATS.items():
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKs 0.05 0.05 0.05\nNs 10\nillum 2\n' + (f'd {OPACITY[n]:.2f}\n' if n in OPACITY else '') + '\n')

def write_obj(name, parts):
    with open(name, 'w') as f:
        f.write('# generated by generate_rv.py\nmtllib rv.mtl\no ' + name[:-4] + '\n')
        off = 0; cnt = 0
        for mat, verts, faces in parts:
            f.write(f'g part{cnt}\nusemtl {mat}\n'); cnt += 1
            for v in verts: f.write('v %.5f %.5f %.5f\n' % v)
            for fc in faces: f.write('f ' + ' '.join(str(i+1+off) for i in fc) + '\n')
            off += len(verts)
        return cnt, off

# the lit lenses: a disc just in front of each headlight (see the loop above),
# drawn emissive while the lights are on. The RV's two spot lights (RV.cpp)
# sit between each pair, at x = +-1.0, y = 0.99
glow = []
for sx in (-1, 1):
    for y in (0.9, 1.08):
        verts = [(sx*1.0 + 0.068*math.cos(2*math.pi*i/16), y + 0.068*math.sin(2*math.pi*i/16), FZ + 0.063) for i in range(16)]
        glow.append(('glow', verts, [list(range(16))]))  # counter-clockwise seen from +z
write_obj('headlight_glow.obj', glow)

# The door, hinged on the front edge of its opening (z = 0.12), turning about a vertical axis through
# HINGE (RV.cpp: DOOR_HINGE): the panel, its window and its handle, in the frame of the hinge
HINGE = (W + 0.02, 0.58, 0.12)
door_parts = []
def dbox(mat, x0, x1, y0, y1, z0, z1):
    box(mat, x0, x1, y0, y1, z0, z1)
    m, verts, faces = objs.pop()
    door_parts.append((m, [(x - HINGE[0], y - HINGE[1], z - HINGE[2]) for x, y, z in verts], faces))
dbox('door', W, W + 0.04, 0.58, 2.47, -0.82, 0.12)
dbox('glass', W + 0.04, W + 0.06, 1.6, 2.35, -0.65, -0.05)
dbox('rack', W + 0.04, W + 0.09, 1.0, 1.08, -0.15, -0.05)  # handle
write_obj('door.obj', door_parts)

# one model per side of the car, centred on the wheel's axle
write_obj('wheel_negx.obj', wheels[-1])
write_obj('wheel_posx.obj', wheels[1])
objs = [o for i, o in enumerate(objs) if i not in set(wheel_parts)]

with open('rv.obj', 'w') as f:
    f.write('# generated by generate_rv.py\nmtllib rv.mtl\no rv\n')
    off = 0; cnt = 0
    for mat, verts, faces in objs:
        f.write(f'g part{cnt}\nusemtl {mat}\n'); cnt += 1
        for v in verts: f.write('v %.5f %.5f %.5f\n' % v)
        for fc in faces: f.write('f ' + ' '.join(str(i+1+off) for i in fc) + '\n')
        off += len(verts)
print(cnt, 'parts,', off, 'vertices')
