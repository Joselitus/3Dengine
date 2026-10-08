"""Generates Bob's ship: a flying saucer (references: 1950s saucers: a wide thin metal disc with a
sharp rim, a raised central dome with a ring of louvres round its base and a glazed cockpit on
top, a smooth underside with a ring of lights). Radius RADIUS metres, metres, Y up; its origin is
the middle of the ground under it when it has landed (the landing legs stand on y = 0): the hull's
underside is at y = LEG_HEIGHT.

Several models, all in that frame, so that the engine can move the parts (Saucer.cpp has the same
numbers, *_ in it):
  saucer_hull.obj     the disc, the dome, the louvres and the cockpit (glass: translucent)
  saucer_lights.obj   the lights of the underside and the rim (drawn glowing)
  saucer_legs.obj     three landing legs, extended (the engine slides them up into the hull)
  saucer_ramp.obj     the ramp, closed (flush with the underside); it hinges at RAMP_HINGE and
                      swings down to the ground (the engine turns it about +X)
  saucer_beam.obj     a cone of light from the underside to the ground (translucent, glowing)
  saucer_gun_mount.obj  the ray gun's mount under the middle: a column and a ball (the engine slides
                      it up into the hull, GUN_TRAVEL, when the gun is put away)
  saucer_gun.obj      the ray gun's barrel, pointing along +z; it turns about GUN_PIVOT (the
                      player's eye when he aims it: the barrel is below and in front of it)
  saucer_shot.obj     the shot: a green rod along +z from the origin, 1 m long (glowing, stretched)
(+ saucer.mtl). Needs numpy. Fixed seed.
"""
import math
import sys

import numpy as np

sys.path.insert(0, '.')
from shapes import Mesh, ellipsoid, tube, write_mtl  # noqa: E402

RADIUS = 4.5
LEG_HEIGHT = 1.6
RAMP_WIDTH, RAMP_LENGTH = 1.1, 2.9
RAMP_HINGE = (0.0, LEG_HEIGHT + 0.08, 0.6)  # (y, z of its near edge; it runs towards +z)
SEG = 40
GUN_PIVOT = (0.0, LEG_HEIGHT - 0.5, 0.0)

MATS = {
    'metal': ((0.70, 0.72, 0.76), 1.0),
    'metal_dark': ((0.30, 0.31, 0.34), 1.0),
    'louvre': ((0.12, 0.12, 0.13), 1.0),
    'glass': ((0.45, 0.65, 0.75), 0.45),
    'light': ((0.75, 1.0, 0.95), 1.0),
    'beam': ((0.65, 0.95, 1.0), 0.18),
    'shot': ((0.45, 1.0, 0.55), 0.85),
}


def lathe(mesh, mat, profile, seg=SEG, y0=0.0):
    """A surface of revolution about +Y from a profile of (radius, height) points, going up and
    round (the normals point away from the axis, or up/down at the ends)."""
    rings = []
    for r, y in profile:
        rings.append([(r * math.cos(2 * math.pi * k / seg), y + y0, r * math.sin(2 * math.pi * k / seg))
                      for k in range(seg)])
    V, N, F = [], [], []
    P = np.array(profile, float)
    for j, ring in enumerate(rings):
        # the profile's normal: perpendicular to its direction there, pointing out
        a = P[max(j - 1, 0)]
        b = P[min(j + 1, len(P) - 1)]
        d = b - a
        n2 = np.array([d[1], -d[0]])  # (dr, dy) turned: outward for a profile going up
        n2 = n2 / (np.linalg.norm(n2) + 1e-12)
        for k, v in enumerate(ring):
            ang = 2 * math.pi * k / seg
            V.append(v)
            N.append((n2[0] * math.cos(ang), n2[1], n2[0] * math.sin(ang)))
    for j in range(len(rings) - 1):
        for k in range(seg):
            a, b = j * seg + k, j * seg + (k + 1) % seg
            F.append([a, (j + 1) * seg + k, (j + 1) * seg + (k + 1) % seg, b])
    mesh.add(mat, V, N, F)


hull = Mesh()
H = LEG_HEIGHT
# the underside (from the middle out) and the rim, then the top up to the dome
lathe(hull, 'metal_dark', [(0.0, 0.0), (1.2, 0.02), (2.6, 0.12), (3.8, 0.30), (RADIUS - 0.2, 0.52)], y0=H)
lathe(hull, 'metal', [(RADIUS - 0.2, 0.52), (RADIUS, 0.62), (RADIUS + 0.05, 0.70), (RADIUS - 0.1, 0.80),
                      (3.6, 1.00), (2.6, 1.18), (2.3, 1.22)], y0=H)
# the ring of louvres round the dome's base, then the dome
lathe(hull, 'metal_dark', [(2.3, 1.22), (2.25, 1.55)], y0=H)
for k in range(36):  # the louvres: thin dark fins standing out of that band
    a = 2 * math.pi * k / 36
    c, s = math.cos(a), math.sin(a)
    ellipsoid(hull, 'louvre', (2.33 * c, H + 1.38, 2.33 * s), (0.02, 0.13, 0.11), nlat=4, nlon=6,
              rot=np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]]))
lathe(hull, 'metal', [(2.25, 1.55), (2.2, 1.62), (1.9, 1.85), (1.2, 2.05), (0.75, 2.12)], y0=H)
# the cockpit: a glazed bubble on top, with a frame
lathe(hull, 'glass', [(0.75, 2.12), (0.74, 2.30), (0.6, 2.50), (0.35, 2.62), (0.0, 2.66)], y0=H)
for k in range(4):
    a = 2 * math.pi * k / 4 + 0.4
    pts = [(0.76 * math.cos(a), H + 2.12, 0.76 * math.sin(a)), (0.6 * math.cos(a), H + 2.5, 0.6 * math.sin(a)),
           (0.05 * math.cos(a), H + 2.67, 0.05 * math.sin(a))]
    tube(hull, 'metal_dark', pts, [0.03, 0.03, 0.02], seg=6, per_segment=3)

lights = Mesh()
for k in range(10):  # a ring of round lights under the hull
    a = 2 * math.pi * k / 10
    ellipsoid(lights, 'light', (3.0 * math.cos(a), H + 0.20, 3.0 * math.sin(a)), (0.22, 0.05, 0.22),
              nlat=5, nlon=10)
for k in range(24):  # small ones along the rim
    a = 2 * math.pi * (k + 0.5) / 24
    ellipsoid(lights, 'light', ((RADIUS + 0.06) * math.cos(a), H + 0.66, (RADIUS + 0.06) * math.sin(a)),
              (0.07, 0.05, 0.07), nlat=4, nlon=6)

legs = Mesh()
for k in range(3):
    a = 2 * math.pi * k / 3 + math.pi / 6
    c, s = math.cos(a), math.sin(a)
    top = np.array([2.9 * c, H + 0.25, 2.9 * s])
    foot = np.array([3.5 * c, 0.08, 3.5 * s])
    tube(legs, 'metal_dark', [top, (top + foot) / 2, foot], [0.09, 0.07, 0.06], seg=8, per_segment=2)
    ellipsoid(legs, 'metal', foot + np.array([0, -0.04, 0]), (0.3, 0.06, 0.3), nlat=5, nlon=10)

# the ramp: a plate closed flush under the hull, from its hinge out towards +z
ramp = Mesh()
y, z0 = RAMP_HINGE[1], RAMP_HINGE[2]
w = RAMP_WIDTH / 2
V = [(-w, y, z0), (w, y, z0), (w, y, z0 + RAMP_LENGTH), (-w, y, z0 + RAMP_LENGTH),
     (-w, y - 0.06, z0), (w, y - 0.06, z0), (w, y - 0.06, z0 + RAMP_LENGTH), (-w, y - 0.06, z0 + RAMP_LENGTH)]


def box_faces(V):
    quads = [([0, 3, 2, 1], (0, 1, 0)), ([4, 5, 6, 7], (0, -1, 0)), ([0, 1, 5, 4], (0, 0, -1)),
             ([2, 3, 7, 6], (0, 0, 1)), ([1, 2, 6, 5], (1, 0, 0)), ([3, 0, 4, 7], (-1, 0, 0))]
    return quads


for quad, n in box_faces(V):  # (each face its own vertices: flat)
    ramp.add('metal_dark', [V[i] for i in quad], [n] * 4, [[0, 1, 2, 3]])
for k in range(6):  # ridges across it, to walk on
    zz = z0 + 0.35 + k * (RAMP_LENGTH - 0.6) / 5
    ellipsoid(ramp, 'metal', (0, y - 0.07, zz), (w * 0.9, 0.015, 0.03), nlat=3, nlon=6)

# the beam: an open cone of light from the underside down to the ground
beam = Mesh()
lathe(beam, 'beam', [(2.6, 0.0), (1.2, H)], y0=0.0)

# the ray gun: a column down from the middle of the underside to a ball above the eye, and a barrel
# below the eye that turns with it
px, py, pz = GUN_PIVOT
mount = Mesh()
tube(mount, 'metal_dark', [(0, H + 0.05, 0), (0, py + 0.35, 0)], [0.09, 0.09], seg=10, per_segment=1)
ellipsoid(mount, 'metal', (0, py + 0.33, 0), (0.2, 0.17, 0.2), nlat=6, nlon=12)
gun = Mesh()
by = py - 0.3  # the barrel's axis, under the eye
for side in (-1, 1):  # two arms from the ball down to the barrel's breech, at its sides
    tube(gun, 'metal_dark', [(0.13 * side, py + 0.25, 0.0), (0.13 * side, by, 0.1)], [0.035, 0.035],
         seg=6, per_segment=1)
ellipsoid(gun, 'metal', (0, by, 0.2), (0.12, 0.1, 0.26), nlat=6, nlon=12)
tube(gun, 'metal_dark', [(0, by, 0.45), (0, by, 1.6)], [0.05, 0.04], seg=10, per_segment=1)
for zz in (0.75, 1.05, 1.35):  # cooling rings
    ellipsoid(gun, 'metal', (0, by, zz), (0.07, 0.07, 0.03), nlat=4, nlon=10)
ellipsoid(gun, 'light', (0, by, 1.61), (0.045, 0.045, 0.02), nlat=4, nlon=8)
shot = Mesh()
tube(shot, 'shot', [(0, 0, 0), (0, 0, 1.0)], [0.05, 0.05], seg=8, per_segment=1)

write_mtl('saucer.mtl', MATS)
hull.write('saucer_hull.obj', 'saucer.mtl', 'saucer_hull')
lights.write('saucer_lights.obj', 'saucer.mtl', 'saucer_lights')
legs.write('saucer_legs.obj', 'saucer.mtl', 'saucer_legs')
ramp.write('saucer_ramp.obj', 'saucer.mtl', 'saucer_ramp')
beam.write('saucer_beam.obj', 'saucer.mtl', 'saucer_beam')
mount.write('saucer_gun_mount.obj', 'saucer.mtl', 'saucer_gun_mount')
gun.write('saucer_gun.obj', 'saucer.mtl', 'saucer_gun')
shot.write('saucer_shot.obj', 'saucer.mtl', 'saucer_shot')
