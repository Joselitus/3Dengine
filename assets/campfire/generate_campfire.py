"""Generates the campfire (Campfire.cpp): a ring of stones round a bed of embers, five logs leant
together over it, two thick logs to sit on beside it, and the flames. Ground on y = 0, the fire's
middle at the origin, metres.

  campfire.obj         the stones, the burning logs, the ash and the two logs to sit on
  campfire_embers.obj  the glowing embers among the ash (drawn glowing)
  campfire_flame.obj   one flame: an orange tongue round a yellow core, its base on the origin (the
                       engine puts three of them on the embers, each turned and flickering)
(+ campfire.mtl). Needs numpy. Seeded (always the same).

Campfire.cpp: SEAT_* (where the logs to sit on are) and FIRE_RADIUS (the stones' ring) are the same
as here.
"""

import math
import sys

import numpy as np

sys.path.insert(0, '.')
from shapes import Mesh, along, ellipsoid, tube, write_mtl  # noqa: E402

FIRE_RADIUS = 0.55          # the ring of stones (to their middles)
SEAT_DISTANCE = 1.9         # the logs to sit on, from the middle
SEAT_ANGLES = (math.radians(200), math.radians(320))  # round +y from +z (towards -z: away from +z)
SEAT_LENGTH, SEAT_R = 1.5, 0.19

MATS = {
    'stone': ((0.46, 0.44, 0.41), 1.0),
    'bark': ((0.30, 0.20, 0.12), 1.0),
    'wood': ((0.62, 0.48, 0.32), 1.0),
    'char': ((0.07, 0.06, 0.05), 1.0),
    'ash': ((0.36, 0.34, 0.32), 1.0),
    'ember': ((1.00, 0.36, 0.06), 1.0),
    'flame': ((1.00, 0.45, 0.08), 0.85),
    'flame_core': ((1.00, 0.85, 0.35), 0.9),
}

rng = np.random.default_rng(20261011)


def log(mesh, a, b, r, bark='bark', ends='wood'):
    """A log from a to b: bark round it, the wood showing at the ends (a little proud of the bark)."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    tube(mesh, bark, [a, a + d * 0.5, b], [r, r * 1.04, r * 0.96], seg=12, per_segment=2)
    u = d / np.linalg.norm(d)
    for p, s in ((a, -1), (b, 1)):
        ellipsoid(mesh, ends, p + u * s * 0.004, (r * 0.93, 0.01, r * 0.93), nlat=4, nlon=12, rot=along(u))


def main():
    body = Mesh()
    # the ring of stones: lumpy, of different sizes
    n = 13
    for i in range(n):
        a = 2 * math.pi * (i + rng.uniform(-0.15, 0.15)) / n
        r = rng.uniform(0.12, 0.17)
        c = (math.sin(a) * FIRE_RADIUS, r * 0.55, math.cos(a) * FIRE_RADIUS)
        ellipsoid(body, 'stone', c, (r * rng.uniform(1.0, 1.3), r * rng.uniform(0.7, 0.95), r),
                  nlat=6, nlon=9, rot=along((math.sin(a), 0.0, math.cos(a))) @ np.diag([1, 1, 1]))
    # the ash inside the ring
    ellipsoid(body, 'ash', (0, 0.0, 0), (0.46, 0.05, 0.46), nlat=6, nlon=18)
    # five logs leant together over the middle, charred where they burn
    for i in range(5):
        a = 2 * math.pi * i / 5 + 0.3
        foot = np.array([math.sin(a) * 0.42, 0.06, math.cos(a) * 0.42])
        top = np.array([math.sin(a) * 0.05, 0.62, math.cos(a) * 0.05])
        log(body, foot, foot + (top - foot) * 0.55, 0.055)
        log(body, foot + (top - foot) * 0.55, top, 0.05, bark='char', ends='char')
    # two thick logs to sit on, lying beside it
    for k, a in enumerate(SEAT_ANGLES):
        c = np.array([math.sin(a) * SEAT_DISTANCE, SEAT_R * 0.9, math.cos(a) * SEAT_DISTANCE])
        side = np.array([math.cos(a), 0.0, -math.sin(a)])  # along the log: round the fire
        log(body, c - side * SEAT_LENGTH / 2, c + side * SEAT_LENGTH / 2, SEAT_R)
    body.write('campfire.obj', 'campfire.mtl', 'campfire')

    embers = Mesh()
    for i in range(16):
        a, r = rng.uniform(0, 2 * math.pi), rng.uniform(0.0, 0.33)
        s = rng.uniform(0.04, 0.08)
        ellipsoid(embers, 'ember', (math.sin(a) * r, 0.04, math.cos(a) * r), (s, s * 0.45, s * 0.8),
                  nlat=4, nlon=7, rot=along((math.cos(a), 0.0, math.sin(a))))
    embers.write('campfire_embers.obj', 'campfire.mtl', 'campfire_embers')

    flame = Mesh()

    def tongue(mat, width, height, lean):
        # a teardrop: round at the base, drawn out to a point at the top, leaning a little
        def shape(p):
            t = (p[1] + 1.0) / 2.0  # 0 at the bottom .. 1 at the top
            if t > 0.3:
                k = 0.95 * ((1.0 - t) / 0.7) ** 1.2
            else:
                k = 0.95 * math.sqrt(max(0.0, 1.0 - ((0.3 - t) / 0.3) ** 2))
            return np.array([p[0] * k + lean * t * t, p[1], p[2] * k])
        ellipsoid(flame, mat, (0, height / 2, 0), (width, height / 2, width), nlat=12, nlon=12, shape=shape)

    tongue('flame', 0.24, 0.95, 0.12)
    tongue('flame_core', 0.12, 0.55, 0.06)
    flame.write('campfire_flame.obj', 'campfire.mtl', 'campfire_flame')
    write_mtl('campfire.mtl', MATS)


main()
