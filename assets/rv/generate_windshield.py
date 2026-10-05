"""Generates the windshield of the RV as two models of their own, so that one can be swapped
for the other when the windshield breaks (see RV::setWindshieldModels):
  windshield.obj / windshield.mtl                 the intact one: two thin translucent panes
  windshield_broken.obj / windshield_broken.mtl   the broken one: the same panes with a texture
  windshield_cracked.png                          the cracked-glass texture (RGBA)
Same frame as rv.obj (Y up, front +Z, meters): the panes sit in the openings of the slanted front face
of the body (see PROF / T0 / T1 / PANES in generate_rv.py: keep them the same). Needs numpy + Pillow.

The texture covers both panes side by side (u across x from -1.05 to 1.05, v up the slant), so it is 2:1.
It is transparent glass with a light tint everywhere (the same as the intact panes) plus a spider
web of cracks around the point of impact, in front of the driver: radial cracks with branches,
concentric ring cracks, a crushed spot at the middle and a milky haze around it. The renderer uses
the alpha of the texture (shader.frag), so the cracks are what you see and the rest stays clear.
"""
import math
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

# ---- the geometry (the same as generate_rv.py)
A, B = (3.55, 1.7), (3.0, 2.95)        # the slanted front face: (z, y) of its bottom and top
NZ, NY = 1.25, 0.55
L = math.hypot(NZ, NY)
NZ, NY = NZ / L, NY / L                # its outward normal
T0, T1 = 0.08, 0.85                    # the part of the face that is glass
PANES = ((-1.05, -0.03), (0.03, 1.05))
X0, X1 = PANES[0][0], PANES[1][1]      # the glass from X0 to X1 (the post is in the middle)


def point(t, x, off=0.0):
    z = A[0] + (B[0] - A[0]) * t
    y = A[1] + (B[1] - A[1]) * t
    return (x, y + NY * off, z + NZ * off)


TINT = (0.55, 0.78, 0.88)
TINT_OPACITY = 0.15

# ---------------------------------------------------------------- the texture
W, H = 1024, 512
SS = 2                                  # draw at twice the size and shrink: smooth lines
random.seed(7)
np.random.seed(7)
IMPACT = (0.62 * W * SS, 0.52 * H * SS)  # in front of the driver (the driver is on +x, to the right in u)

tint = tuple(int(c * 255) for c in TINT)
img = Image.new('RGBA', (W * SS, H * SS), tint + (int(TINT_OPACITY * 255),))
draw = ImageDraw.Draw(img, 'RGBA')


def line(points, width, colour):
    draw.line(points, fill=colour, width=max(1, int(round(width))), joint='curve')


# a milky haze around the impact: the glass is crushed and scatters the light
haze = np.zeros((H * SS, W * SS), dtype=np.float32)
yy, xx = np.mgrid[0:H * SS, 0:W * SS]
dist = np.hypot(xx - IMPACT[0], yy - IMPACT[1])
haze = np.clip(1.0 - dist / (190 * SS), 0, 1) ** 1.6
hz = np.asarray(img).astype(np.float32)
hz[..., 3] = np.clip(hz[..., 3] + haze * 70, 0, 255)
hz[..., :3] = hz[..., :3] * (1 - haze[..., None] * 0.55) + np.array([225, 238, 244], np.float32) * (haze[..., None] * 0.55)
img = Image.fromarray(hz.astype(np.uint8), 'RGBA')
draw = ImageDraw.Draw(img, 'RGBA')

CRACK = (236, 246, 250, 235)
SHADE = (40, 70, 90, 110)               # a dark thin line under each crack: some depth


def crack(points, w0, w1):
    """A crack that tapers from width w0 to w1 along its points."""
    n = len(points) - 1
    for i in range(n):
        w = w0 + (w1 - w0) * i / max(n - 1, 1)
        seg = [points[i], points[i + 1]]
        line([(x + 1.5 * SS, y + 1.5 * SS) for x, y in seg], w, SHADE)
        line(seg, w, CRACK)


def walk(start, angle, length, step, wobble):
    pts = [start]
    x, y = start
    travelled = 0.0
    while travelled < length:
        s = step * random.uniform(0.6, 1.4)
        angle += random.uniform(-wobble, wobble)
        x += math.cos(angle) * s
        y += math.sin(angle) * s
        pts.append((x, y))
        travelled += s
    return pts


# radial cracks from the impact, evenly spread with some jitter
RADIALS = 17
radial_pts = []
for k in range(RADIALS):
    ang = 2 * math.pi * k / RADIALS + random.uniform(-0.12, 0.12)
    length = random.choice([260, 340, 430, 560, 760, 1100]) * SS
    pts = walk(IMPACT, ang, length, 26 * SS, 0.10)
    radial_pts.append(pts)
    crack(pts, 5.0 * SS, 1.4 * SS)
    # branches off the radial crack
    for _ in range(random.randint(2, 4)):
        i = random.randint(2, max(3, len(pts) - 2))
        side = random.choice((-1, 1))
        a = ang + side * random.uniform(0.45, 1.0)
        branch = walk(pts[i], a, random.uniform(70, 230) * SS, 18 * SS, 0.16)
        crack(branch, 2.8 * SS, 0.9 * SS)

# concentric ring cracks: chords between neighbouring radial cracks at a given distance
for radius in (46, 80, 128, 190, 270, 380):
    r = radius * SS
    pts_at = []
    for pts in radial_pts:
        best = None
        for p in pts:
            d = math.hypot(p[0] - IMPACT[0], p[1] - IMPACT[1])
            if best is None or abs(d - r) < abs(best[1] - r):
                best = (p, d)
        pts_at.append(best[0] if best and abs(best[1] - r) < 40 * SS else None)
    for k in range(RADIALS):
        a, b = pts_at[k], pts_at[(k + 1) % RADIALS]
        if a and b and random.random() < 0.78:
            mid = ((a[0] + b[0]) / 2 + random.uniform(-6, 6) * SS, (a[1] + b[1]) / 2 + random.uniform(-6, 6) * SS)
            crack([a, mid, b], 2.4 * SS, 1.3 * SS)

# the crushed spot at the impact: small bright splinters
for _ in range(34):
    ang = random.uniform(0, 2 * math.pi)
    d = abs(random.gauss(0, 16)) * SS
    cx, cy = IMPACT[0] + math.cos(ang) * d, IMPACT[1] + math.sin(ang) * d
    size = random.uniform(2, 7) * SS
    poly = [(cx + math.cos(a) * size * random.uniform(0.5, 1.2), cy + math.sin(a) * size * random.uniform(0.5, 1.2))
            for a in (0, 2.1, 4.2)]
    draw.polygon(poly, fill=(245, 250, 252, 225))

# a few small stray cracks near the edges (the shock runs through the whole glass)
for _ in range(7):
    edge = random.choice(('top', 'bottom', 'left', 'right'))
    x = random.uniform(0, W * SS) if edge in ('top', 'bottom') else (0 if edge == 'left' else W * SS)
    y = random.uniform(0, H * SS) if edge in ('left', 'right') else (0 if edge == 'top' else H * SS)
    toward = math.atan2(IMPACT[1] - y, IMPACT[0] - x)
    crack(walk((x, y), toward + random.uniform(-0.4, 0.4), random.uniform(60, 180) * SS, 16 * SS, 0.2),
          2.4 * SS, 0.9 * SS)

img = img.resize((W, H), Image.LANCZOS)
img.save('windshield_cracked.png', optimize=True)

# ------------------------------------------------------------------- the models
def box(verts, faces, a, b):
    """A thin box between the quads a (4 points) and b (4 points)."""
    base = len(verts)
    verts.extend(a + b)
    for f in ([0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]):
        faces.append([base + i for i in f])


def volume(verts, faces):
    s = 0
    for f in faces:
        a = verts[f[0]]
        for i in range(1, len(f) - 1):
            b, c = verts[f[i]], verts[f[i + 1]]
            s += (a[0]*(b[1]*c[2]-b[2]*c[1]) - a[1]*(b[0]*c[2]-b[2]*c[0]) + a[2]*(b[0]*c[1]-b[1]*c[0]))
    return s


# intact: two thin boxes (as they were in rv.obj)
verts, faces = [], []
for xa, xb in PANES:
    d = 0.02
    box(verts, faces,
        [point(T0, xa, -d / 2), point(T0, xb, -d / 2), point(T1, xb, -d / 2), point(T1, xa, -d / 2)],
        [point(T0, xa, d / 2), point(T0, xb, d / 2), point(T1, xb, d / 2), point(T1, xa, d / 2)])
if volume(verts, faces) < 0:
    faces = [f[::-1] for f in faces]
with open('windshield.mtl', 'w') as f:
    f.write('# generated by generate_windshield.py\nnewmtl windshield\n'
            f'Ka {TINT[0]:.3f} {TINT[1]:.3f} {TINT[2]:.3f}\nKd {TINT[0]:.3f} {TINT[1]:.3f} {TINT[2]:.3f}\n'
            f'Ks 0.05 0.05 0.05\nNs 10\nillum 2\nd {TINT_OPACITY:.2f}\n')
with open('windshield.obj', 'w') as f:
    f.write('# generated by generate_windshield.py\nmtllib windshield.mtl\no windshield\ng intact\nusemtl windshield\n')
    for v in verts:
        f.write('v %.5f %.5f %.5f\n' % v)
    for fc in faces:
        f.write('f ' + ' '.join(str(i + 1) for i in fc) + '\n')

# broken: one quad per pane with the texture (both sides are drawn: no culling)
with open('windshield_broken.mtl', 'w') as f:
    f.write('# generated by generate_windshield.py\nnewmtl windshield_cracked\n'
            'Ka 1.0 1.0 1.0\nKd 1.0 1.0 1.0\nKs 0.05 0.05 0.05\nNs 10\nillum 2\n'
            'd 0.99\nmap_Kd windshield_cracked.png\n')   # (d < 1: it goes with the translucent meshes)
with open('windshield_broken.obj', 'w') as f:
    f.write('# generated by generate_windshield.py\nmtllib windshield_broken.mtl\no windshield_broken\n'
            'g broken\nusemtl windshield_cracked\n')
    n = 0
    for xa, xb in PANES:
        quad = [point(T0, xa), point(T0, xb), point(T1, xb), point(T1, xa)]
        uvs = [((x - X0) / (X1 - X0), (t - T0) / (T1 - T0)) for x, t in ((xa, T0), (xb, T0), (xb, T1), (xa, T1))]
        for v in quad:
            f.write('v %.5f %.5f %.5f\n' % v)
        for u, v in uvs:
            f.write('vt %.5f %.5f\n' % (u, v))
        f.write('f ' + ' '.join(f'{n + i + 1}/{n + i + 1}' for i in range(4)) + '\n')
        n += 4
print('windshield.obj, windshield_broken.obj and windshield_cracked.png written')
