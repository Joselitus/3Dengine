"""Generates the garden gnome (an enemy: Gnome.cpp). 0.84 m tall, feet on y = 0, facing +Z, metres.

Several models, all in the gnome's body frame (standing, arms down), so that the engine can move
the limbs and swap the face (Gnome.cpp: HIP_*, SHOULDER and the HEAD numbers are the same as here):
  gnome_body.obj      coat, belt, left arm, head, nose, ears, beard and hat
  gnome_arm_r.obj     the right arm, hanging (turns about SHOULDER_R when he runs with the knife)
  gnome_knife.obj     the knife, in the right fist, blade down (it turns with the arm)
  gnome_leg_l/r.obj   trousers and boot (turn about the hip)
  gnome_face_0..4.obj the five faces (eyes, brows, mouth), one is shown at a time:
                        0 beaming, 1 smile fading, 2 annoyed, 3 furious, 4 a wide smile with
                        white eyes (the next look sets him off)
  gnome_eyes_white.obj the glowing white eyes of face 4 (drawn unlit)
(+ gnome.mtl). Needs numpy. No randomness.
"""
import math
import sys

import numpy as np

sys.path.insert(0, '.')
from shapes import Mesh, ellipsoid, tube, write_mtl  # noqa: E402

SHOULDER_R = (-0.095, 0.43, 0.0)
HIP_L = (0.045, 0.22, 0.0)
HEAD = np.array([0.0, 0.55, 0.02])
HEAD_R = np.array([0.095, 0.10, 0.09])  # radii of the head

MATS = {
    'coat': ((0.13, 0.28, 0.72), 1.0),
    'hat': ((0.80, 0.08, 0.08), 1.0),
    'skin': ((0.92, 0.68, 0.58), 1.0),
    'nose': ((0.95, 0.48, 0.44), 1.0),
    'beard': ((0.95, 0.95, 0.93), 1.0),
    'belt': ((0.12, 0.07, 0.04), 1.0),
    'buckle': ((0.85, 0.70, 0.20), 1.0),
    'trousers': ((0.80, 0.76, 0.58), 1.0),
    'boot': ((0.25, 0.14, 0.07), 1.0),
    'glove': ((0.95, 0.95, 0.95), 1.0),
    'eye_white': ((0.97, 0.97, 0.97), 1.0),
    'pupil': ((0.03, 0.03, 0.03), 1.0),
    'brow': ((0.22, 0.17, 0.17), 1.0),
    'mouth': ((0.45, 0.05, 0.07), 1.0),
    'teeth': ((0.98, 0.98, 0.95), 1.0),
    'flush': ((0.88, 0.22, 0.22), 1.0),
    'eye_glow': ((1.0, 1.0, 1.0), 1.0),
    'steel': ((0.78, 0.80, 0.84), 1.0),
    'handle': ((0.30, 0.17, 0.08), 1.0),
}


def surf(x, y, off=0.0):
    """A point on the front of the head at (x, y) from its middle, `off` metres out."""
    k = 1.0 - (x / HEAD_R[0]) ** 2 - (y / HEAD_R[1]) ** 2
    z = HEAD_R[2] * math.sqrt(max(k, 0.02))
    return HEAD + np.array([x, y, z + off])


# ------------------------------------------------------------------------------ the body
body = Mesh()
tube(body, 'coat', [(0, 0.19, 0), (0, 0.32, 0), (0, 0.46, 0.0)], [0.10, 0.108, 0.075], seg=16)
ellipsoid(body, 'belt', (0, 0.27, 0.0), (0.111, 0.016, 0.111), nlat=6, nlon=16)
ellipsoid(body, 'buckle', (0, 0.27, 0.108), (0.022, 0.018, 0.008), nlat=6, nlon=8)
# left arm (the viewer's right as he faces us): sleeve and a white glove
tube(body, 'coat', [(0.09, 0.43, 0), (0.115, 0.34, 0.01), (0.12, 0.27, 0.03)], [0.03, 0.027, 0.025], seg=10)
ellipsoid(body, 'glove', (0.12, 0.25, 0.035), (0.024, 0.026, 0.024), nlat=6, nlon=8)
# head, ears, nose
ellipsoid(body, 'skin', HEAD, HEAD_R, nlat=14, nlon=20)
for s in (1, -1):
    ellipsoid(body, 'skin', HEAD + np.array([0.092 * s, -0.005, -0.01]), (0.014, 0.03, 0.02), nlat=6, nlon=8)
ellipsoid(body, 'nose', surf(0, -0.012, 0.012), (0.024, 0.026, 0.024), nlat=8, nlon=12)
# beard: white, hanging to the belt
ellipsoid(body, 'beard', (0, 0.40, 0.075), (0.082, 0.085, 0.048), nlat=8, nlon=14)
ellipsoid(body, 'beard', (0, 0.34, 0.088), (0.060, 0.085, 0.040), nlat=8, nlon=14)
ellipsoid(body, 'beard', (0, 0.28, 0.095), (0.036, 0.065, 0.030), nlat=8, nlon=14)
for s in (1, -1):
    ellipsoid(body, 'beard', (0.07 * s, 0.49, 0.045), (0.028, 0.055, 0.045), nlat=6, nlon=10)
# hat: a red cone, leaning back a little
tube(body, 'hat', [(0, 0.635, 0.01), (0, 0.70, 0.0), (0, 0.775, -0.02), (0, 0.84, -0.055)],
     [0.108, 0.085, 0.045, 0.006], seg=16)
ellipsoid(body, 'hat', (0, 0.635, 0.012), (0.112, 0.012, 0.108), nlat=4, nlon=16)

# the right arm (turns about the shoulder) and the knife in its fist
arm = Mesh()
tube(arm, 'coat', [SHOULDER_R, (-0.115, 0.34, 0.01), (-0.12, 0.27, 0.03)], [0.03, 0.027, 0.025], seg=10)
ellipsoid(arm, 'glove', (-0.12, 0.25, 0.035), (0.024, 0.026, 0.024), nlat=6, nlon=8)
knife = Mesh()
tube(knife, 'handle', [(-0.12, 0.275, 0.035), (-0.12, 0.235, 0.035)], [0.011, 0.011], seg=8, per_segment=1)
ellipsoid(knife, 'buckle', (-0.12, 0.233, 0.035), (0.016, 0.004, 0.016), nlat=4, nlon=8)
# the blade, flat and pointing down, with a point
ellipsoid(knife, 'steel', (-0.12, 0.155, 0.035), (0.0035, 0.085, 0.015), nlat=8, nlon=8)

# the legs: trousers and a boot, turning about the hip
legs = {}
for side, s in (('l', 1), ('r', -1)):
    leg = Mesh()
    tube(leg, 'trousers', [(0.045 * s, 0.23, 0), (0.045 * s, 0.12, 0), (0.045 * s, 0.05, 0)],
         [0.042, 0.038, 0.034], seg=10)
    ellipsoid(leg, 'boot', (0.045 * s, 0.032, 0.025), (0.037, 0.032, 0.062), nlat=8, nlon=12)
    legs[f'gnome_leg_{side}.obj'] = leg

# ------------------------------------------------------------------------------ the faces
# per face: eye half-height, brow (outer y, middle y, inner y, thickness), mouth curve (+ = smile)
# and where it sits, cheek flush
FACES = [
    dict(eye=0.023, pupil=0.012, brow=(0.052, 0.064, 0.056, 0.0050), curve=28.0, mouth_y=-0.046, flush=0.0),
    dict(eye=0.022, pupil=0.011, brow=(0.046, 0.050, 0.046, 0.0050), curve=14.0, mouth_y=-0.043, flush=0.0),
    dict(eye=0.017, pupil=0.010, brow=(0.046, 0.040, 0.032, 0.0058), curve=-3.0, mouth_y=-0.038, flush=0.4),
    dict(eye=0.012, pupil=0.0075, brow=(0.060, 0.044, 0.024, 0.0070), curve=-26.0, mouth_y=-0.030, flush=1.0),
    dict(eye=0.025, pupil=0.0, brow=(0.050, 0.060, 0.052, 0.0045), curve=44.0, mouth_y=-0.050, flush=0.0),
]
faces = []
glow = Mesh()
for n, f in enumerate(FACES):
    m = Mesh()
    for s in (1, -1):
        ex = 0.040 * s
        if n == 4:
            ellipsoid(glow, 'eye_glow', surf(ex, 0.012, 0.002), (0.019, f['eye'], 0.009), nlat=8, nlon=12)
        else:
            ellipsoid(m, 'eye_white', surf(ex, 0.012, 0.002), (0.019, f['eye'], 0.009), nlat=8, nlon=12)
            ellipsoid(m, 'pupil', surf(ex, 0.010, 0.008), (f['pupil'], f['pupil'] * 1.2, 0.004), nlat=6, nlon=8)
        # the brow, from the outside in
        outer, mid, inner, thick = f['brow']
        tube(m, 'brow', [surf(0.066 * s, outer, 0.006), surf(0.040 * s, mid, 0.008), surf(0.014 * s, inner, 0.010)],
             [thick * 0.8, thick, thick * 0.8], seg=6, per_segment=3)
        if f['flush'] > 0:
            ellipsoid(m, 'flush', surf(0.062 * s, -0.022, 0.003),
                      (0.020 * (0.4 + 0.6 * f['flush']), 0.014 * (0.4 + 0.6 * f['flush']), 0.006), nlat=6, nlon=8)
    # the mouth: a curve under the nose, over the beard; the corners are at +-0.032
    pts = []
    for x in np.linspace(-0.040, 0.040, 7):
        pts.append(surf(x, f['mouth_y'] + f['curve'] * x * x - 0.005 * f['curve'] * 0, 0.006))
    if n == 4:
        # teeth: a white band along the top of the grin
        top = [p + np.array([0, 0.0016, 0.0]) for p in pts[1:-1]]
        tube(m, 'teeth', top, [0.0065] * len(top), seg=6, per_segment=2)
    tube(m, 'mouth', pts, [0.0042, 0.0050, 0.0055, 0.0058, 0.0055, 0.0050, 0.0042], seg=6, per_segment=3)
    faces.append(m)

write_mtl('gnome.mtl', MATS)
body.write('gnome_body.obj', 'gnome.mtl', 'gnome_body')
arm.write('gnome_arm_r.obj', 'gnome.mtl', 'gnome_arm_r')
knife.write('gnome_knife.obj', 'gnome.mtl', 'gnome_knife')
for name, leg in legs.items():
    leg.write(name, 'gnome.mtl', name[:-4])
for n, m in enumerate(faces):
    m.write(f'gnome_face_{n}.obj', 'gnome.mtl', f'gnome_face_{n}')
glow.write('gnome_eyes_white.obj', 'gnome.mtl', 'gnome_eyes_white')
