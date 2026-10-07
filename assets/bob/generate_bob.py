"""Generates Bob, the alien (references: grey aliens: a big bulbous hairless head, huge black
almond eyes, two slits for a nose, a small lipless mouth, a thin neck and a thin body, long arms
with long fingers). 1.45 m tall, feet on y = 0, facing +Z, metres (like the rest of the project).

Bob is several models, all in the same frame (his body's, standing with the arms down), so that
the engine can move his limbs (Bob.cpp: the joints JOINT_* have the same numbers as here):
  bob_body.obj        torso, neck, head, nose and mouth
  bob_eyes.obj        the eyes, black and glossy (by day)
  bob_eyes_glow.obj   the same eyes in white, drawn glowing at night
  bob_upperarm_l/r, bob_forearm_l/r (with the hand), bob_thigh_l/r, bob_shin_l/r (with the foot)
  bob_ray.obj         the ray from his eyes: a thin yellow rod along +z, 1 m long (translucent)
(+ bob.mtl). +X is Bob's left. Needs numpy. Fixed seed.
"""
import math
import sys

import numpy as np

sys.path.insert(0, '.')
from shapes import Mesh, along, ellipsoid, rot_x, tube, write_mtl  # noqa: E402

# the joints (left side; the right one is its mirror image)
SHOULDER = (0.165, 1.02, 0.0)
ELBOW = (0.19, 0.76, -0.01)
WRIST = (0.20, 0.52, 0.01)
HIP = (0.085, 0.64, 0.0)
KNEE = (0.09, 0.34, 0.02)
ANKLE = (0.09, 0.06, 0.0)
HEAD = (0.0, 1.23, 0.02)  # the middle of the head

MATS = {
    'skin': ((0.50, 0.53, 0.55), 1.0),
    'skin_dark': ((0.20, 0.21, 0.22), 1.0),  # nostrils and mouth
    'eye': ((0.02, 0.02, 0.025), 1.0),
    'eye_glow': ((0.97, 0.98, 1.0), 1.0),
    'ray': ((1.0, 0.88, 0.15), 0.7),
}


def mirror_point(p):
    return (-p[0], p[1], p[2])


def head_shape(p):
    """The unit sphere made into a grey's head: a wide, high cranium bulging at the back, and a
    narrow chin."""
    x, y, z = p
    up = max(0.0, y)
    down = max(0.0, -y)
    width = 1.0 + 0.10 * up - 0.45 * down ** 1.3      # wide skull, narrow jaw
    depth = 1.0 + 0.08 * up - 0.25 * down
    zz = z * depth + (0.18 * up * (1 - z) if z < 0 else 0.0) * 0.5  # the back of the skull bulges
    return np.array([x * width, y * (1.0 + 0.08 * up), zz - 0.12 * down * z])


body = Mesh()
# the head
ellipsoid(body, 'skin', HEAD, (0.17, 0.21, 0.18), nlat=18, nlon=24, shape=head_shape)
# the neck, thin
tube(body, 'skin', [(0, 1.02, 0.0), (0, 1.07, 0.0), (0, 1.12, 0.01)], [0.045, 0.04, 0.045], seg=10)
# the torso: narrow shoulders, a thin chest, a slight belly, the pelvis
tube(body, 'skin', [(0, 0.66, 0.0), (0, 0.78, 0.01), (0, 0.92, 0.0), (0, 1.03, -0.005)],
     [0.105, 0.10, 0.115, 0.08], seg=14)
ellipsoid(body, 'skin', (0, 0.995, 0.0), (0.195, 0.055, 0.075), nlat=8, nlon=14)  # the shoulders
ellipsoid(body, 'skin', (0, 0.66, 0.0), (0.12, 0.07, 0.085), nlat=8, nlon=14)  # the hips
# the nose: two small slits; the mouth: a thin dark line
for s in (1, -1):
    ellipsoid(body, 'skin_dark', (0.012 * s, 1.165, 0.185), (0.006, 0.009, 0.004), nlat=4, nlon=6)
ellipsoid(body, 'skin_dark', (0.0, 1.105, 0.170), (0.03, 0.004, 0.006), nlat=4, nlon=8)

# the eyes: big almonds slanting up and out, wrapping round the face
eyes, glow = Mesh(), Mesh()
for s in (1, -1):
    tilt = rot_x(0.0)
    a = 0.38 * s  # the slant
    c, sn = math.cos(a), math.sin(a)
    slant = np.array([[c, -sn, 0], [sn, c, 0], [0, 0, 1]])
    turn = np.array([[math.cos(0.42 * s), 0, math.sin(0.42 * s)], [0, 1, 0],
                     [-math.sin(0.42 * s), 0, math.cos(0.42 * s)]])
    rot = turn @ slant @ tilt
    centre = (0.072 * s, 1.235, 0.163)
    ellipsoid(eyes, 'eye', centre, (0.058, 0.032, 0.035), nlat=10, nlon=14, rot=rot)
    ellipsoid(glow, 'eye_glow', centre, (0.059, 0.033, 0.036), nlat=10, nlon=14, rot=rot)

limbs = {}
for side, f in (('l', lambda p: p), ('r', mirror_point)):
    up, fore = Mesh(), Mesh()
    tube(up, 'skin', [f(SHOULDER), f(ELBOW)], [0.035, 0.026], seg=10, per_segment=2)
    ellipsoid(up, 'skin', f(ELBOW), (0.028, 0.028, 0.028), nlat=6, nlon=8)
    tube(fore, 'skin', [f(ELBOW), f(WRIST)], [0.026, 0.02], seg=10, per_segment=2)
    # the hand: a narrow palm and three long thin fingers, hanging
    w = np.array(f(WRIST))
    ellipsoid(fore, 'skin', w + np.array([0, -0.035, 0]), (0.02, 0.04, 0.03), nlat=6, nlon=8)
    for k, dz in enumerate((-0.022, 0.0, 0.022)):
        tip = w + np.array([0.0, -0.16 - 0.02 * (k == 1), dz * 1.3])
        tube(fore, 'skin', [w + np.array([0, -0.06, dz]), (w + tip) / 2 + np.array([0, -0.03, dz * 0.3]), tip],
             [0.008, 0.007, 0.004], seg=6, per_segment=3)
    thigh, shin = Mesh(), Mesh()
    tube(thigh, 'skin', [f(HIP), f(KNEE)], [0.05, 0.036], seg=10, per_segment=2)
    ellipsoid(thigh, 'skin', f(KNEE), (0.036, 0.036, 0.036), nlat=6, nlon=8)
    tube(shin, 'skin', [f(KNEE), f(ANKLE)], [0.034, 0.026], seg=10, per_segment=2)
    # the foot: long and narrow
    a = np.array(f(ANKLE))
    ellipsoid(shin, 'skin', a + np.array([0, -0.03, 0.05]), (0.035, 0.025, 0.09), nlat=6, nlon=10)
    limbs[f'bob_upperarm_{side}.obj'] = up
    limbs[f'bob_forearm_{side}.obj'] = fore
    limbs[f'bob_thigh_{side}.obj'] = thigh
    limbs[f'bob_shin_{side}.obj'] = shin

# the paralysing ray: a thin glowing rod along +z, 1 m long (the engine stretches it from each eye
# to the player's head)
ray = Mesh()
tube(ray, 'ray', [(0, 0, 0), (0, 0, 1.0)], [0.022, 0.016], seg=8, per_segment=1)

write_mtl('bob.mtl', MATS)
ray.write('bob_ray.obj', 'bob.mtl', 'bob_ray')
body.write('bob_body.obj', 'bob.mtl', 'bob_body')
eyes.write('bob_eyes.obj', 'bob.mtl', 'bob_eyes')
glow.write('bob_eyes_glow.obj', 'bob.mtl', 'bob_eyes_glow')
for name, mesh in limbs.items():
    mesh.write(name, 'bob.mtl', name[:-4])
