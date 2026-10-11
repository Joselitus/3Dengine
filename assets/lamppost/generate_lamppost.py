"""Generates the desert lamp post (scenery: LampPost.cpp) and the lamp post that is alive
(an enemy: LampStalker.cpp), which looks exactly the same. Feet on y = 0, the arm towards +Z,
metres.

  lamppost.obj        concrete footing, steel pole, the arm and the lamp's head
  lamppost_glass.obj  the lamp's glass, under its head (drawn glowing at night)
  lamppost_foot.obj   one of the living post's four tiny feet: a pale foot and a thin leg up to
                      LEG_TOP, the sole's middle at the origin (the engine puts four of them under
                      the footing: hidden inside it while it stands, out when it runs)
(+ lamppost.mtl). Needs numpy. No randomness.

LampStalker.cpp: FOOTING_HEIGHT, FOOT_RING (where the feet are, from the middle) and LEG_TOP
are the same as here.
"""

import sys


sys.path.insert(0, '.')
from shapes import Mesh, ellipsoid, tube, write_mtl  # noqa: E402

FOOTING_R, FOOTING_HEIGHT = 0.28, 0.6
POLE_TOP = 7.6
ARM_REACH = 1.7
FOOT_RING = 0.13
LEG_TOP = 0.5

MATS = {
    'concrete': ((0.62, 0.60, 0.56), 1.0),
    'steel': ((0.52, 0.54, 0.56), 1.0),
    'head': ((0.30, 0.31, 0.33), 1.0),
    'glass': ((0.98, 0.92, 0.70), 1.0),
    'skin': ((0.86, 0.70, 0.64), 1.0),
    'nail': ((0.95, 0.90, 0.82), 1.0),
}


def cylinder(mesh, mat, radius, y0, y1, seg=16, taper=1.0):
    tube(mesh, mat, [(0, y0, 0), (0, y1, 0)], [radius, radius * taper], seg=seg, per_segment=1)


def post():
    m = Mesh()
    cylinder(m, 'concrete', FOOTING_R, 0.0, FOOTING_HEIGHT, seg=20)
    cylinder(m, 'concrete', FOOTING_R * 0.8, FOOTING_HEIGHT, FOOTING_HEIGHT + 0.12, seg=20)
    cylinder(m, 'steel', 0.11, FOOTING_HEIGHT, POLE_TOP, seg=14, taper=0.65)
    # the arm: up from the top of the pole and out over the road, bending down a little at the end
    tube(m, 'steel', [(0, POLE_TOP - 0.3, 0), (0, POLE_TOP + 0.15, 0.35), (0, POLE_TOP + 0.3, 1.0),
                      (0, POLE_TOP + 0.22, ARM_REACH)], [0.07, 0.06, 0.055, 0.05], seg=10, per_segment=6)
    # the head: a flat, long shell
    ellipsoid(m, 'head', (0, POLE_TOP + 0.18, ARM_REACH + 0.2), (0.2, 0.1, 0.38), nlat=8, nlon=16)
    m.write('lamppost.obj', 'lamppost.mtl', 'lamppost')
    g = Mesh()
    ellipsoid(g, 'glass', (0, POLE_TOP + 0.11, ARM_REACH + 0.2), (0.15, 0.04, 0.3), nlat=6, nlon=14)
    g.write('lamppost_glass.obj', 'lamppost.mtl', 'lamppost_glass')


def foot():
    m = Mesh()
    tube(m, 'skin', [(0, 0.05, -0.02), (0, 0.25, -0.01), (0, LEG_TOP, 0)], [0.035, 0.03, 0.03], seg=8,
         per_segment=4)
    ellipsoid(m, 'skin', (0, 0.035, 0.03), (0.055, 0.04, 0.1), nlat=6, nlon=12)
    for k in (-1, 0, 1):  # three little toes with pale nails
        ellipsoid(m, 'skin', (k * 0.03, 0.025, 0.12), (0.017, 0.017, 0.025), nlat=5, nlon=8)
        ellipsoid(m, 'nail', (k * 0.03, 0.035, 0.135), (0.011, 0.006, 0.012), nlat=4, nlon=8)
    m.write('lamppost_foot.obj', 'lamppost.mtl', 'lamppost_foot')


if __name__ == '__main__':
    write_mtl('lamppost.mtl', MATS)
    post()
    foot()
    print('FOOTING_HEIGHT', FOOTING_HEIGHT, 'FOOT_RING', FOOT_RING, 'LEG_TOP', LEG_TOP,
          'top', POLE_TOP + 0.3, 'reach', ARM_REACH)

