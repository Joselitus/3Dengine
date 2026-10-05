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
    for x, flip in ((-W, False), (W, True)):
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
    sbox('teal', 1.3, 1.4, -3.55, 3.55)
    sbox('orange', 1.2, 1.3, -3.55, 3.55)
    for z0, z1 in ((-3.1,-2.2),(-1.9,-1.0)):
        sbox('glass', 1.6, 2.35, z0, z1, 0.0, 0.03)
    sbox('glass', 1.9, 2.5, 0.45, 0.9, 0.0, 0.03)  # small cab-side window
    if sx > 0:
        # door only on the +X side
        sbox('door', 0.6, 2.45, -0.8, 0.1, 0.0, 0.04)
        sbox('glass', 1.6, 2.35, -0.65, -0.05, 0.04, 0.06)
        sbox('rack', 1.0, 1.08, -0.15, -0.05, 0.04, 0.09)  # handle
    else:
        sbox('glass', 1.6, 2.35, -0.7, 0.0, 0.0, 0.03)
    # mirror: arm + head
    sbox('mirror', 1.97, 2.03, 3.34, 3.40, 0.0, 0.2)
    sbox('mirror', 1.80, 2.22, 3.32, 3.44, 0.2, 0.3)
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
