"""Generates folla_culos.obj / folla_culos.mtl: a horrible night creature, T-posing.
References: reference_01jpg.jpg (a clay figure: bald, gaunt, long arms, long clawed hands, brown skin
with white dust) and reference_02.webp (a render: emaciated, long thin limbs, a smooth skull with
deep black eyes and a wide grin of small sharp teeth, no nose, ribs showing, huge talons).

Meters, Y up, facing +Z (like the rest of the project), feet on y = 0, centred between the feet.
2.3 m tall, 3.9 m across the claws. T-pose: the arms straight out along +-X with the palms down
(the thumbs forward), the legs straight and a little apart, the head up and forward.
Built from tubes (varying radius along a smooth path), ellipsoids and a lathe-like torso; every
part has smooth normals (written as `vn`), so it looks organic though it is low poly.
Needs numpy. Check it with f3d (see CLAUDE.md), not with the engine.
"""
import math
import random

import numpy as np

random.seed(13)
pieces = []  # (material, vertices (n, 3), normals (n, 3), faces)


def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


def add_piece(mat, V, N, F, closed=True):
    """Adds a mesh piece. Its faces are turned, if need be, to agree with its normals (which point
    outwards by construction): it works for open pieces too, where the volume would not tell."""
    V = np.asarray(V, float)
    N = np.asarray(N, float)
    agree = 0.0
    for f in F:
        fn = np.cross(V[f[1]] - V[f[0]], V[f[2]] - V[f[0]])
        agree += float(np.dot(fn, N[f[0]]))
    if agree < 0:
        F = [f[::-1] for f in F]
    pieces.append((mat, V, N, F))


# ------------------------------------------------------------------------- curves
def catmull(points, per_segment=6):
    """A smooth curve (Catmull-Rom) through the control points (any dimension)."""
    P = [np.asarray(p, float) for p in points]
    if len(P) < 3:
        return [P[0] + (P[-1] - P[0]) * t for t in np.linspace(0, 1, per_segment + 1)]
    out = []
    ext = [2 * P[0] - P[1]] + P + [2 * P[-1] - P[-2]]
    for i in range(1, len(ext) - 2):
        p0, p1, p2, p3 = ext[i - 1], ext[i], ext[i + 1], ext[i + 2]
        for k in range(per_segment):
            t = k / per_segment
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
                              (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(P[-1])
    return out


# ------------------------------------------------------------------------- shapes
def grid_mesh(P, centres):
    """A closed-around surface from rings P[j][i] (ring j, point i): smooth normals pointing away from
    each ring's centre. Returns vertices, normals and quads."""
    nj, ni = P.shape[0], P.shape[1]
    N = np.zeros_like(P)
    for j in range(nj):
        for i in range(ni):
            du = P[j, (i + 1) % ni] - P[j, (i - 1) % ni]
            dv = P[min(j + 1, nj - 1), i] - P[max(j - 1, 0), i]
            n = np.cross(du, dv)
            out = P[j, i] - centres[j]
            if np.linalg.norm(n) < 1e-10:
                n = out
            if np.dot(n, out) < 0:
                n = -n
            N[j, i] = unit(n)
    F = []
    for j in range(nj - 1):
        for i in range(ni):
            a, b = j * ni + i, j * ni + (i + 1) % ni
            c, d = (j + 1) * ni + (i + 1) % ni, (j + 1) * ni + i
            F.append([a, b, c, d])
    return P.reshape(-1, 3), N.reshape(-1, 3), F


def tube(mat, ctrl, radii, seg=12, per_segment=5, squash=1.0, cap_start=True, cap_end=True, up=None):
    """A tube along a smooth path through `ctrl`, its radius going through `radii` (one per control
    point; a tiny last radius makes a point). `squash` < 1 flattens the section (the second axis)."""
    path = catmull(ctrl, per_segment)
    rad = [r[0] for r in catmull([[r] for r in radii], per_segment)]
    K = len(path)
    rings = np.zeros((K, seg, 3))
    t_prev = None
    u = None
    for k in range(K):
        t = unit(path[min(k + 1, K - 1)] - path[max(k - 1, 0)])
        if u is None:
            ref = np.array([0.0, 1.0, 0.0]) if up is None else np.asarray(up, float)
            if abs(np.dot(ref, t)) > 0.9:
                ref = np.array([0.0, 0.0, 1.0])
            u = unit(ref - t * np.dot(ref, t))
        else:
            u = unit(u - t * np.dot(u, t))
        v = np.cross(t, u)
        for i in range(seg):
            a = 2 * math.pi * i / seg
            rings[k, i] = path[k] + rad[k] * (math.cos(a) * u + squash * math.sin(a) * v)
    V, N, F = grid_mesh(rings, path)
    V, N = list(V), list(N)

    def cap(k, direction):
        base = len(V)
        for i in range(seg):
            V.append(rings[k, i])
            N.append(direction)
        V.append(path[k])
        N.append(direction)
        for i in range(seg):
            F.append([base + i, base + seg, base + (i + 1) % seg] if direction is None else
                     [base + (i + 1) % seg, base + seg, base + i])
    if cap_start:
        cap(0, -unit(path[1] - path[0]))
    if cap_end:
        cap(K - 1, unit(path[-1] - path[-2]))
    add_piece(mat, V, N, F)


def ellipsoid(mat, centre, radii, nlat=10, nlon=16, rot=None, shape=None):
    """An ellipsoid (pole to pole along Y). `shape(x, y, z)` may scale a point of the unit sphere."""
    c = np.asarray(centre, float)
    r = np.asarray(radii, float)
    V, N, F = [], [], []
    for j in range(nlat + 1):
        th = math.pi * j / nlat
        for i in range(nlon):
            ph = 2 * math.pi * i / nlon
            p = np.array([math.sin(th) * math.sin(ph), math.cos(th), math.sin(th) * math.cos(ph)])
            if shape:
                p = shape(p)
            q = p * r
            n = unit(p / r)
            if rot is not None:
                q, n = rot @ q, rot @ n
            V.append(c + q)
            N.append(unit(n))
    for j in range(nlat):
        for i in range(nlon):
            a, b = j * nlon + i, j * nlon + (i + 1) % nlon
            F.append([a, b, (j + 1) * nlon + (i + 1) % nlon, (j + 1) * nlon + i])
    add_piece(mat, V, N, F)


def mirrored(points):
    return [np.array([-p[0], p[1], p[2]]) for p in points]


# ---------------------------------------------------------------------- the torso
# profile: (y, half width x, half depth z); the abdomen is hollow, the chest narrow
TORSO = [(1.00, 0.125, 0.085), (1.10, 0.135, 0.095), (1.24, 0.095, 0.075), (1.42, 0.115, 0.085),
         (1.60, 0.150, 0.100), (1.75, 0.185, 0.100), (1.86, 0.170, 0.090), (1.94, 0.075, 0.070), (1.97, 0.05, 0.05)]
dense = np.array(catmull(TORSO, 12))
RIBS = [1.40 + 0.040 * k for k in range(9)]


def torso_radius(y):
    return np.interp(y, dense[:, 0], dense[:, 1]), np.interp(y, dense[:, 0], dense[:, 2])


NJ, NI = 46, 30
ys = np.linspace(1.00, 1.97, NJ)
ring = np.zeros((NJ, NI, 3))
centres = []
for j, y in enumerate(ys):
    rx, rz = torso_radius(y)
    for i in range(NI):
        a = 2 * math.pi * i / NI                   # 0 = the front (+z)
        bump = sum(math.exp(-((y - yr) / 0.011) ** 2) for yr in RIBS) * 0.022
        front = max(0.0, math.cos(a - 0.0)) ** 0.5 * (0.35 + 0.65 * abs(math.sin(a)) ** 0.5)   # the ribs wrap round the sides
        sternum = 0.006 * math.exp(-(math.sin(a) / 0.18) ** 2) * (1.0 if 1.40 < y < 1.80 else 0.0) * max(0.0, math.cos(a))
        ring[j, i] = (math.sin(a) * (rx + bump * front * 0.6), y, math.cos(a) * (rz + bump * front + sternum))
    centres.append(np.array([0.0, y, 0.0]))
V, N, F = grid_mesh(ring, centres)
V, N = list(V), list(N)
for k, direction in ((0, np.array([0.0, -1.0, 0.0])), (NJ - 1, np.array([0.0, 1.0, 0.0]))):
    base = len(V)
    for i in range(NI):
        V.append(ring[k, i])
        N.append(direction)
    V.append(centres[k])
    N.append(direction)
    for i in range(NI):
        F.append([base + i, base + NI, base + (i + 1) % NI] if k else [base + (i + 1) % NI, base + NI, base + i])
add_piece('skin', V, N, F)

# the pelvis and its bones, the collarbones, the knobs of the spine
ellipsoid('skin', (0, 1.06, 0), (0.15, 0.10, 0.10))
for s in (-1, 1):
    ellipsoid('skin', (s * 0.125, 1.13, 0.01), (0.060, 0.052, 0.065))                    # hip bones
    tube('skin', [(s * 0.03, 1.87, 0.085), (s * 0.13, 1.895, 0.07), (s * 0.235, 1.875, 0.04)], [0.010, 0.012, 0.012], seg=8)
for k in range(13):
    y = 1.22 + 0.056 * k
    rz = torso_radius(y)[1]
    ellipsoid('skin', (0, y, -rz + 0.004), (0.016, 0.014, 0.014), nlat=5, nlon=8)

# ----------------------------------------------------------------------- the head
HEAD = np.array([0.0, 2.17, 0.03])


def skull(p):    # the skull narrows towards the chin
    x, y, z = p
    k = 1.0 - 0.26 * max(0.0, -y) ** 1.4
    return np.array([x * k, y, z * (1.0 - 0.10 * max(0.0, -y))])


ellipsoid('skin', HEAD, (0.112, 0.158, 0.132), nlat=14, nlon=20, shape=skull)
ellipsoid('skin', (0, 2.075, 0.065), (0.078, 0.075, 0.085), nlat=10, nlon=16)           # the jaw
tube('skin', [(0, 1.84, 0.0), (0, 1.95, 0.005), (0, 2.03, 0.02)], [0.054, 0.047, 0.050], seg=12)   # the long neck
for s in (-1, 1):
    ellipsoid('skin', (s * 0.066, 2.115, 0.108), (0.026, 0.020, 0.022), nlat=6, nlon=10)  # cheekbones
    ellipsoid('skin', (s * 0.045, 2.236, 0.122), (0.043, 0.013, 0.016), nlat=6, nlon=10)  # brow ridges
    ellipsoid('eye', (s * 0.046, 2.195, 0.131), (0.034, 0.026, 0.019), nlat=8, nlon=12)   # the black eyes, deep in their sockets
# the mouth: a dark wide slit with two rows of small sharp teeth
ellipsoid('mouth', (0, 2.072, 0.126), (0.064, 0.019, 0.014), nlat=6, nlon=14)
for row, (y, direction) in enumerate(((2.088, -1.0), (2.056, 1.0))):
    n = 17
    for k in range(n):
        x = -0.066 + 0.132 * k / (n - 1)
        z = 0.065 + 0.085 * math.sqrt(max(0.0, 1 - (x / 0.078) ** 2)) + 0.002
        length = random.uniform(0.013, 0.022) * (1.25 if k % 4 == 2 else 1.0)
        lean = random.uniform(-0.2, 0.2) * 0.01
        tube('bone', [(x, y, z), (x + lean, y + direction * length * 0.5, z + 0.002),
                      (x + lean, y + direction * length, z + 0.004)], [0.0038, 0.0028, 0.0003], seg=6, per_segment=2,
             cap_end=True, cap_start=False)

# ---------------------------------------------------------------------- the arms
SHOULDER_Y = 1.84
for s in (-1, 1):
    ellipsoid('skin', (s * 0.255, SHOULDER_Y, 0.0), (0.065, 0.060, 0.060), nlat=8, nlon=12)         # the shoulder
    arm = [(s * 0.25, SHOULDER_Y, 0.0), (s * 0.54, SHOULDER_Y, -0.012), (s * 0.82, SHOULDER_Y, -0.02),
           (s * 1.10, SHOULDER_Y, -0.008), (s * 1.38, SHOULDER_Y, 0.0)]
    tube('skin', arm, [0.050, 0.038, 0.036, 0.030, 0.021], seg=12, squash=0.9)
    ellipsoid('skin', (s * 0.82, SHOULDER_Y, -0.02), (0.042, 0.040, 0.040), nlat=6, nlon=10)        # the elbow
    # the hand: a thin palm, palm down, thumb forward
    ellipsoid('skin', (s * 1.47, SHOULDER_Y - 0.004, 0.0), (0.075, 0.016, 0.062), nlat=6, nlon=10)
    FINGERS = [  # (z of the knuckle, spread angle (rad) away from +x towards +z, phalanx lengths)
        (0.046, 0.34, (0.105, 0.085, 0.070)), (0.016, 0.12, (0.125, 0.095, 0.080)),
        (-0.016, -0.12, (0.115, 0.090, 0.075)), (-0.046, -0.34, (0.095, 0.075, 0.062))]
    for z0, spread, lengths in FINGERS:
        pos = np.array([s * 1.525, SHOULDER_Y - 0.004, z0])
        heading = np.array([s * math.cos(spread), 0.0, math.sin(spread)])
        pts, rads, droop = [pos.copy()], [0.0125], 0.0
        for k, ln in enumerate(lengths):
            droop += 0.30                                             # each joint bends the finger down
            d = unit(heading + np.array([0.0, -math.sin(droop) * 0.8, 0.0]))
            pos = pos + d * ln
            pts.append(pos.copy())
            rads.append(0.0115 - 0.0022 * (k + 1))
        tube('skin', pts, rads, seg=8, per_segment=3, cap_end=False)
        tip = pts[-1]
        d = unit(pts[-1] - pts[-2])
        # the talon: long, thin, pale, curving down and forward
        talon = [tip, tip + d * 0.07 + np.array([0, -0.012, 0.004]), tip + d * 0.125 + np.array([0, -0.045, 0.012]),
                 tip + d * 0.15 + np.array([0, -0.095, 0.02])]
        tube('bone', talon, [0.0085, 0.0062, 0.0036, 0.0003], seg=8, per_segment=3, cap_start=False)
    # the thumb: short, pointing forward and out
    base = np.array([s * 1.46, SHOULDER_Y - 0.004, 0.045])
    heading = unit(np.array([s * 0.35, -0.1, 1.0]))
    pts = [base, base + heading * 0.06, base + heading * 0.115, base + heading * 0.16 + np.array([0, -0.02, 0])]
    tube('skin', pts, [0.0135, 0.0115, 0.0092, 0.0075], seg=8, per_segment=3, cap_end=False)
    tip = pts[-1]
    d = unit(pts[-1] - pts[-2])
    tube('bone', [tip, tip + d * 0.05 + np.array([0, -0.01, 0]), tip + d * 0.1 + np.array([0, -0.04, 0.01]),
                  tip + d * 0.12 + np.array([0, -0.085, 0.015])], [0.0075, 0.0055, 0.0032, 0.0003], seg=8, per_segment=3,
         cap_start=False)

# ---------------------------------------------------------------------- the legs
for s in (-1, 1):
    leg = [(s * 0.115, 1.14, 0.0), (s * 0.130, 0.88, 0.012), (s * 0.150, 0.60, 0.02), (s * 0.160, 0.35, 0.0),
           (s * 0.170, 0.115, -0.012)]
    tube('skin', leg, [0.078, 0.058, 0.046, 0.040, 0.027], seg=14, squash=0.95)
    ellipsoid('skin', (s * 0.150, 0.60, 0.03), (0.049, 0.052, 0.052), nlat=8, nlon=12)             # the knee
    ellipsoid('skin', (s * 0.172, 0.10, -0.012), (0.034, 0.034, 0.036), nlat=6, nlon=10)           # the ankle
    ellipsoid('skin', (s * 0.172, 0.052, 0.065), (0.055, 0.048, 0.150), nlat=8, nlon=12)           # the foot
    ellipsoid('skin', (s * 0.172, 0.040, -0.085), (0.044, 0.040, 0.050), nlat=6, nlon=10)          # the heel
    for k, dx in enumerate((-0.036, -0.012, 0.012, 0.036, 0.058)):
        x0 = s * (0.172 + dx * (1 if s > 0 else 1))
        longer = (0.17, 0.20, 0.19, 0.16, 0.12)[k]
        toe = [(x0, 0.032, 0.185), (x0 + s * dx * 0.5, 0.020, 0.185 + longer * 0.5),
               (x0 + s * dx * 1.0, 0.014, 0.185 + longer)]
        r0 = 0.017 if k == 4 else 0.0135
        tube('skin', toe, [r0, r0 * 0.78, r0 * 0.6], seg=8, per_segment=3, cap_end=False)
        tip = np.array(toe[-1])
        tube('bone', [tip, tip + np.array([0, -0.003, 0.014]), tip + np.array([0, -0.01, 0.03])],
             [0.007, 0.0048, 0.0004], seg=6, per_segment=2, cap_start=False)

# ------------------------------------------------------------------------ output
MATS = {
    'skin': (0.76, 0.76, 0.74),     # ash white-grey, dry and tight over the bones
    'bone': (0.50, 0.46, 0.38),     # the teeth and the talons: darker, yellowed bone, so they stand out
    'eye': (0.01, 0.01, 0.015),     # the black pits of the eyes
    'mouth': (0.03, 0.012, 0.012),  # the inside of the mouth
}
with open('folla_culos.mtl', 'w') as f:
    f.write('# generated by generate_folla_culos.py\n')
    for n, c in MATS.items():
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\n'
                'Ks 0.10 0.10 0.10\nNs 20\nillum 2\n\n')
total_v = total_f = 0
with open('folla_culos.obj', 'w') as f:
    f.write('# generated by generate_folla_culos.py\nmtllib folla_culos.mtl\no folla_culos\n')
    off = 0
    for cnt, (mat, V, N, F) in enumerate(pieces):
        f.write(f'g part{cnt}\nusemtl {mat}\n')
        for v in V:
            f.write('v %.5f %.5f %.5f\n' % tuple(v))
        for n in N:
            f.write('vn %.4f %.4f %.4f\n' % tuple(n))
        for fc in F:
            f.write('f ' + ' '.join(f'{i + 1 + off}//{i + 1 + off}' for i in fc) + '\n')
        off += len(V)
        total_v += len(V)
        total_f += len(F)
allv = np.vstack([p[1] for p in pieces])
print(len(pieces), 'parts,', total_v, 'vertices,', total_f, 'faces')
print('bbox min', allv.min(0).round(3), 'max', allv.max(0).round(3))
