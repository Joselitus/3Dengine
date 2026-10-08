"""Generates the Flatwoods monster (references: the 1952 Flatwoods sightings and their drawings: a
tall figure that floats, a dark pleated skirt down to the ground, a metallic chest, thin arms
with long claws, a round pale face with two big round eyes, and behind its head a tall dark hood
shaped like the spade of a pack of cards). 2.6 m tall from the hem of its skirt (which floats
over the ground), facing +Z, metres, Y up (like the rest of the project).

Two models, in the same frame:
  flatwoods_body.obj   skirt, chest, arms and claws, face and hood
  flatwoods_eyes.obj   its two round eyes, red (drawn glowing)
(+ flatwoods.mtl). Needs numpy. No randomness.
"""
import math
import sys

import numpy as np

sys.path.insert(0, '../bob')
from shapes import Mesh, ellipsoid, tube, unit, write_mtl  # noqa: E402

SEG = 48
MATS = {
    'cloth': ((0.10, 0.13, 0.11), 1.0),   # the skirt: dark green-black
    'metal': ((0.42, 0.45, 0.48), 1.0),   # the chest
    'claw': ((0.08, 0.08, 0.09), 1.0),    # arms and claws
    'face': ((0.55, 0.52, 0.46), 1.0),
    'hood': ((0.05, 0.06, 0.07), 1.0),
    'eye': ((1.0, 0.08, 0.04), 1.0),
}

SKIRT_TOP, CHEST_TOP = 1.3, 1.85
FACE = np.array([0.0, 2.08, 0.08])  # the middle of the face


def lathe(mesh, mat, profile, seg=SEG, radius_of=None):
    """A surface of revolution about +Y from (radius, height) points going up; `radius_of(r, a)`
    may change the radius round it (the pleats)."""
    P = np.array(profile, float)
    V, N, F = [], [], []
    for j, (r, y) in enumerate(P):
        a0 = P[max(j - 1, 0)]
        b0 = P[min(j + 1, len(P) - 1)]
        d = b0 - a0
        n2 = unit(np.array([d[1], -d[0]]))
        for k in range(seg):
            ang = 2 * math.pi * k / seg
            rr = radius_of(r, ang) if radius_of else r
            V.append((rr * math.sin(ang), y, rr * math.cos(ang)))
            N.append((n2[0] * math.sin(ang), n2[1], n2[0] * math.cos(ang)))
    for j in range(len(P) - 1):
        for k in range(seg):
            a, b = j * seg + k, j * seg + (k + 1) % seg
            F.append([a, b, (j + 1) * seg + (k + 1) % seg, (j + 1) * seg + k])
    mesh.add(mat, V, N, F)


body = Mesh()
# the skirt: wide at the hem, narrowing to the waist, with deep pleats that fade upwards
pleats = lambda r, a: r * (1.0 + 0.06 * math.cos(14 * a))
lathe(body, 'cloth', [(0.0, 0.02), (0.62, 0.0), (0.6, 0.15), (0.52, 0.6), (0.42, 1.0), (0.33, SKIRT_TOP)],
      radius_of=pleats)
# the chest: a rounded metal barrel, wider at the shoulders, a collar round the neck
lathe(body, 'metal', [(0.34, SKIRT_TOP - 0.03), (0.38, 1.5), (0.4, 1.7), (0.33, CHEST_TOP), (0.14, 1.93), (0.0, 1.94)])
lathe(body, 'claw', [(0.36, SKIRT_TOP - 0.06), (0.37, SKIRT_TOP - 0.02), (0.37, SKIRT_TOP + 0.04), (0.35, SKIRT_TOP + 0.06)])

# the arms: thin, from the shoulders out and down, the forearms reaching forward, long claws
for side in (-1, 1):
    shoulder = np.array([0.36 * side, 1.74, 0.0])
    elbow = np.array([0.5 * side, 1.42, 0.08])
    wrist = np.array([0.44 * side, 1.3, 0.42])
    tube(body, 'claw', [shoulder, elbow, wrist], [0.05, 0.035, 0.03], seg=8, per_segment=4)
    ellipsoid(body, 'claw', wrist + np.array([0, -0.01, 0.04]), (0.05, 0.035, 0.07), nlat=5, nlon=8)
    for k in (-1, 0, 1):  # three claws, curling down
        base = wrist + np.array([0.035 * k, -0.01, 0.08])
        mid = base + np.array([0.04 * k, -0.04, 0.13])
        tip = mid + np.array([0.02 * k, -0.12, 0.05])
        tube(body, 'claw', [base, mid, tip], [0.016, 0.011, 0.002], seg=6, per_segment=3)

# the face: a round pale disc-like head
ellipsoid(body, 'face', FACE, (0.25, 0.26, 0.15), nlat=10, nlon=16)

# the hood: the spade of a pack of cards behind the head, slightly cupped round it
def spade(t):
    """The outline of the spade, t in 0..1 round it, x across and y up (centred on the face)."""
    a = 2 * math.pi * t
    # a heart upside down: wide lobes at the bottom, a point at the top
    x = 0.5 * math.sin(a) ** 3 * 1.05
    y = -(0.40 * math.cos(a) - 0.16 * math.cos(2 * a) - 0.06 * math.cos(3 * a) - 0.02 * math.cos(4 * a))
    return x, y * 1.15 + 0.12


K = 72
outline = [spade(k / K) for k in range(K)]
cx, cy = FACE[0], FACE[1]
for face_side, dz in (('front', 1), ('back', -1)):
    V, N, F = [], [], []
    for (x, y) in outline:
        cup = -0.18 * (x * x) / 0.25  # cupped forward round the head
        z = FACE[2] - 0.12 + cup + (0.0 if dz > 0 else -0.04)
        V.append((cx + x, cy + y, z))
        N.append((0.0, 0.0, dz))
    V.append((cx, cy + 0.1, FACE[2] - 0.12 + (0.0 if dz > 0 else -0.04)))
    N.append((0.0, 0.0, dz))
    c = len(V) - 1
    for k in range(K):
        F.append([c, k, (k + 1) % K] if dz > 0 else [c, (k + 1) % K, k])
    body.add('hood', V, N, F)
# its rim
V, N, F = [], [], []
for (x, y) in outline:
    cup = -0.18 * (x * x) / 0.25
    for dzz in (0.0, -0.04):
        V.append((cx + x, cy + y, FACE[2] - 0.12 + cup + dzz))
        N.append(tuple(unit(np.array([x, y - 0.12, 0.0]))))
for k in range(K):
    a, b = 2 * k, 2 * ((k + 1) % K)
    F.append([a, b, b + 1, a + 1])
body.add('hood', V, N, F)

eyes = Mesh()
for side in (-1, 1):
    ellipsoid(eyes, 'eye', FACE + np.array([0.1 * side, 0.03, 0.125]), (0.08, 0.08, 0.04), nlat=8, nlon=12)

write_mtl('flatwoods.mtl', MATS)
body.write('flatwoods_body.obj', 'flatwoods.mtl', 'flatwoods_body')
eyes.write('flatwoods_eyes.obj', 'flatwoods.mtl', 'flatwoods_eyes')
