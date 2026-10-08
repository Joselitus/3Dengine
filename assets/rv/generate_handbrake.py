"""Generates handbrake_base.obj and handbrake_lever.obj (+ .mtl): the RV's handbrake, an ordinary car
one, beside the driver's right knee: a lever whose shaft comes straight out of the floor through a
leather boot, a flat steel bar with a moulded black grip and a chrome release button on its end (the
thumb presses it to pull the lever up or let it go). The lever is a separate model so that RV.cpp can
swing it with the state of the brake.

Both are in the PIVOT's frame (the lever turns about its origin, just under the top of the boot): Y up,
+Z towards the front of the vehicle, +X along the pivot's axis (the RV's x). The lever points along +Y in
its own frame; RV.cpp tips it FORWARD (towards +Z, as in a car: the lever leans towards the dashboard
when it is down and is pulled up and back to apply the brake) by HANDBRAKE_RELEASED (55 degrees from
upright) / HANDBRAKE_ENGAGED (15) degrees and puts both at HANDBRAKE_PIVOT (RV frame; keep them the same
as PIVOT below). Meters, real size (50 cm from the pivot to the end of the grip).
"""
import math

PIVOT = (0.12, 0.62, 1.75)  # in the RV's frame: beside the driver's right knee, on the floor (y = 0.55) under the boot

base_objs, lever_objs = [], []   # (material, verts, faces)
target = base_objs


def vol(verts, faces):
    s = 0
    for f in faces:
        a = verts[f[0]]
        for i in range(1, len(f) - 1):
            b, c = verts[f[i]], verts[f[i + 1]]
            s += (a[0]*(b[1]*c[2]-b[2]*c[1]) - a[1]*(b[0]*c[2]-b[2]*c[0]) + a[2]*(b[0]*c[1]-b[1]*c[0]))
    return s


def add(mat, verts, faces):
    """Closed solids only: the faces are turned to face outwards."""
    if vol(verts, faces) < 0:
        faces = [f[::-1] for f in faces]
    target.append((mat, list(verts), faces))


def box(mat, x0, x1, y0, y1, z0, z1):
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1),
         (x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)]
    add(mat, v, [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]])


def prism(mat, prof, x0, x1):
    """The profile (z, y points) extruded along x (any simple polygon)."""
    n = len(prof)
    verts = [(x0, y, z) for z, y in prof] + [(x1, y, z) for z, y in prof]
    faces = [list(range(n)), list(range(2*n-1, n-1, -1))]
    for i in range(n):
        j = (i + 1) % n
        faces.append([i, n+i, n+j, j])
    add(mat, verts, faces)


def cyl_y(mat, cx, cz, r, y0, y1, seg=20):
    """A cylinder standing along y."""
    verts = [(cx + r*math.cos(2*math.pi*i/seg), y, cz + r*math.sin(2*math.pi*i/seg))
             for y in (y0, y1) for i in range(seg)]
    faces = [list(range(seg)), list(range(2*seg-1, seg-1, -1))]
    for i in range(seg):
        j = (i + 1) % seg
        faces.append([i, seg+i, seg+j, j])
    add(mat, verts, faces)


def disc_x(mat, cy, cz, r, x0, x1, seg=20):
    """A cylinder lying along x (the pivot pin)."""
    prism(mat, [(cz + r*math.cos(2*math.pi*i/seg), cy + r*math.sin(2*math.pi*i/seg)) for i in range(seg)], x0, x1)


# ------------------------------------------------------------------- the base (does not move)
# the shaft comes straight out of the floor (y = -0.07 here) through a leather boot: a squat truncated
# pyramid with a second fold above it, on a flat trim ring screwed to the floor
def frustum(mat, y0, y1, hx0, hz0, hx1, hz1):
    v = [(-hx0, y0, -hz0), (hx0, y0, -hz0), (hx0, y0, hz0), (-hx0, y0, hz0),
         (-hx1, y1, -hz1), (hx1, y1, -hz1), (hx1, y1, hz1), (-hx1, y1, hz1)]
    add(mat, v, [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]])
box('plastic', -0.100, 0.100, -0.070, -0.058, -0.100, 0.100)
frustum('leather', -0.058, -0.010, 0.080, 0.080, 0.032, 0.032)
frustum('leather', -0.010, 0.030, 0.032, 0.032, 0.021, 0.021)

# ------------------------------------------------------------------- the lever (it swings)
target = lever_objs
# the flat steel bar (in the plane it swings in: wide there, thin across), a little narrower at the top
prism('metal', [(-0.016, -0.04), (0.016, -0.04), (0.011, 0.40), (-0.011, 0.40)], -0.0032, 0.0032)


def loft(mat, rings, seg=24):
    """A solid through oval rings (y, half width along x, half depth along z), capped at both ends."""
    verts = [(rx * math.cos(2*math.pi*i/seg), y, rz * math.sin(2*math.pi*i/seg))
             for y, rx, rz in rings for i in range(seg)]
    faces = [list(range(seg)), list(range(seg*len(rings) - 1, seg*(len(rings) - 1) - 1, -1))]
    for r in range(len(rings) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            faces.append([r*seg + i, (r+1)*seg + i, (r+1)*seg + j, r*seg + j])
    add(mat, verts, faces)


# the moulded grip: an oval about 13 cm long, fat in the middle, tapering at both ends, with three
# shallow finger grooves, slightly rounded at the top (where the button is)
GRIP0, GRIP1, N = 0.36, 0.50, 14
rings = []
for k in range(N + 1):
    t = k / N
    belly = 0.80 + 0.20 * math.sin(math.pi * min(max(t * 1.05, 0.0), 1.0))   # fat in the middle
    groove = sum(0.07 * math.exp(-((t - c) / 0.035) ** 2) for c in (0.30, 0.50, 0.70))
    round_top = 1.0 if t < 0.93 else 0.80                                    # the top edge rounded off
    f = belly * (1.0 - groove) * round_top
    rings.append((GRIP0 + (GRIP1 - GRIP0) * t, 0.0175 * f, 0.0140 * f))
loft('grip', rings)
# the release button, on the end of the grip: chrome, a little proud of it
box('chrome', -0.0105, 0.0105, GRIP1 - 0.001, GRIP1 + 0.007, -0.0075, 0.0075)

MATS = {'plastic': (0.13, 0.13, 0.14), 'slot': (0.01, 0.01, 0.01), 'leather': (0.16, 0.09, 0.05),
        'metal': (0.45, 0.45, 0.47), 'grip': (0.07, 0.07, 0.08), 'chrome': (0.82, 0.82, 0.80)}


def write(name, objs):
    with open(name + '.mtl', 'w') as f:
        f.write('# generated by generate_handbrake.py\n')
        for n, c in MATS.items():
            f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\n'
                    'Ks 0.20 0.20 0.20\nNs 30\nillum 2\n\n')
    with open(name + '.obj', 'w') as f:
        f.write(f'# generated by generate_handbrake.py\nmtllib {name}.mtl\no {name}\n')
        off = 0
        for cnt, (mat, verts, faces) in enumerate(objs):
            f.write(f'g part{cnt}\nusemtl {mat}\n')
            for v in verts:
                f.write('v %.5f %.5f %.5f\n' % v)
            for fc in faces:
                f.write('f ' + ' '.join(str(i + 1 + off) for i in fc) + '\n')
            off += len(verts)
        print(name, len(objs), 'parts,', off, 'vertices')


write('handbrake_base', base_objs)
write('handbrake_lever', lever_objs)
