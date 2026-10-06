"""Generates steering_wheel.obj / .mtl: the RV's steering wheel and column, a model of its own
(so it can be turned later). Meters, real size: a big thin rim like a 70s bus/RV, three spokes
(left, right, bottom), a domed hub and a column that comes out from under the dashboard.

Its own frame: the origin is the centre of the hub, the rim lies in the XY plane, +Z is the
steering axis pointing out of the column towards the driver (the column goes towards -Z), +Y is
the "12 o'clock" of the wheel and +X = Y x Z. Spokes: -X, +X and -Y. Turning the wheel is a
rotation about Z.
Placement in the RV (same frame as rv.obj) is written to steering_wheel_mounts.json.
"""
import json
import math

objs = []  # (material, verts, faces)

TILT_DEG = 55.0              # how far the steering axis points above the horizontal
RIM_R, RIM_T = 0.270, 0.0155  # rim: centre-line radius and tube radius
COLUMN_LEN = 0.55


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
    objs.append((mat, list(verts), faces))


def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def norm(a):
    l = math.sqrt(sum(x*x for x in a)); return tuple(x/l for x in a)


def tube(mat, path, radii, seg=14, closed=False, scale_xy=(1.0, 1.0)):
    """A tube along a polyline `path` with a radius per point. Not closed: capped at both ends."""
    n = len(path)
    verts, rings = [], []
    prev_n = None
    for i, p in enumerate(path):
        if closed:
            t = norm(sub(path[(i+1) % n], path[(i-1) % n]))
        else:
            t = norm(sub(path[min(i+1, n-1)], path[max(i-1, 0)]))
        # a stable frame: carry the normal along the path
        ref = prev_n if prev_n else ((0, 0, 1) if abs(t[2]) < 0.9 else (1, 0, 0))
        u = norm(cross(t, cross(ref, t)))
        w = cross(t, u)
        prev_n = u
        ring = []
        for k in range(seg):
            a = 2*math.pi*k/seg
            c, s = math.cos(a)*radii[i]*scale_xy[0], math.sin(a)*radii[i]*scale_xy[1]
            ring.append(len(verts))
            verts.append((p[0]+u[0]*c+w[0]*s, p[1]+u[1]*c+w[1]*s, p[2]+u[2]*c+w[2]*s))
        rings.append(ring)
    faces = []
    last = n if closed else n-1
    for i in range(last):
        a, b = rings[i], rings[(i+1) % n]
        for k in range(seg):
            m = (k+1) % seg
            faces.append([a[k], b[k], b[m], a[m]])
    if not closed:
        faces.append(rings[0])
        faces.append(rings[-1][::-1])
    add(mat, verts, faces)


def revolve(mat, prof, seg=36):
    """A solid of revolution about Z from a (radius, z) profile; the first and last points must be
    on the axis (radius 0)."""
    verts, rings = [], []
    for r, z in prof:
        ring = []
        for k in range(seg):
            a = 2*math.pi*k/seg
            ring.append(len(verts))
            verts.append((r*math.cos(a), r*math.sin(a), z))
        rings.append(ring)
    faces = []
    for i in range(len(prof)-1):
        for k in range(seg):
            m = (k+1) % seg
            a, b = rings[i], rings[i+1]
            if prof[i][0] == 0:
                faces.append([a[0], b[m], b[k]])
            elif prof[i+1][0] == 0:
                faces.append([a[k], a[m], b[0]])
            else:
                faces.append([a[k], a[m], b[m], b[k]])
    add(mat, verts, faces)


DISH = 0.045   # the hub sits this much toward the driver (+Z) relative to the rim plane (z = 0)

# ------------------------------------------------------------------ the rim (a torus)
SEG = 72
rim = [(RIM_R*math.cos(2*math.pi*i/SEG), RIM_R*math.sin(2*math.pi*i/SEG), 0.0) for i in range(SEG)]
tube('rubber', rim, [RIM_T]*SEG, seg=14, closed=True)

# ------------------------------------------------------------------ the spokes
# each goes from the hub to the rim, flat (wider than thick) and dished: z falls from DISH to 0
def spoke(angle_deg, width=0.040, thick=0.016):
    a = math.radians(angle_deg)
    d = (math.cos(a), math.sin(a))
    pts, rad = [], []
    N = 9
    for i in range(N):
        t = i / (N-1)
        r = 0.045 + (RIM_R - 0.045 + 0.004) * t
        z = DISH * (1 - t) ** 1.6
        pts.append((d[0]*r, d[1]*r, z))
        rad.append(1.0)
    # elliptical section: wide in the wheel plane, thin along the dish direction
    verts_before = len(objs)
    tube('rubber', pts, [width/2*(1 - 0.25*i/(N-1)) for i in range(N)], seg=12, scale_xy=(thick/width, 1.0))
for ang in (180, 0, 270):
    spoke(ang)

# ------------------------------------------------------------------ hub and horn pad
revolve('rubber', [(0, DISH+0.026), (0.030, DISH+0.022), (0.052, DISH+0.010), (0.058, DISH-0.004),
                   (0.050, DISH-0.020), (0.0, DISH-0.020)])
revolve('chrome', [(0, DISH+0.0275), (0.022, DISH+0.024), (0.026, DISH+0.020), (0.0, DISH+0.020)])

# ------------------------------------------------------------------ the column
# a collar under the hub, then a slightly tapered column that goes to -Z, with a boot at the dash
revolve('column', [(0, DISH-0.020), (0.040, DISH-0.020), (0.036, DISH-0.060), (0.030, DISH-0.090),
                   (0.027, -0.14), (0.0, -0.14)])
revolve('column', [(0.0, -0.13), (0.026, -0.13), (0.024, -COLUMN_LEN+0.10), (0.0, -COLUMN_LEN+0.10)])
# the column shroud ends in a rubber boot (flange) where it enters the dashboard
revolve('rubber', [(0.0, -COLUMN_LEN+0.115), (0.024, -COLUMN_LEN+0.115), (0.050, -COLUMN_LEN+0.085),
                   (0.066, -COLUMN_LEN+0.040), (0.070, -COLUMN_LEN), (0.0, -COLUMN_LEN)])

MATS = {'rubber': (0.07, 0.07, 0.075), 'chrome': (0.78, 0.78, 0.80), 'column': (0.14, 0.14, 0.15)}
with open('steering_wheel.mtl', 'w') as f:
    f.write('# generated by generate_steering_wheel.py\n')
    for n, c in MATS.items():
        sp = 0.6 if n == 'chrome' else 0.2
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\n'
                f'Ks {sp} {sp} {sp}\nNs 40\nillum 2\n\n')
with open('steering_wheel.obj', 'w') as f:
    f.write('# generated by generate_steering_wheel.py\nmtllib steering_wheel.mtl\no steering_wheel\n')
    off = 0
    for cnt, (mat, verts, faces) in enumerate(objs):
        f.write(f'g part{cnt}\nusemtl {mat}\n')
        for v in verts:
            f.write('v %.5f %.5f %.5f\n' % v)
        for fc in faces:
            f.write('f ' + ' '.join(str(i + 1 + off) for i in fc) + '\n')
        off += len(verts)
print(len(objs), 'parts,', off, 'vertices')

# ------------------------------------------------------------------ placement in the RV
# Driver's seat at x 0.45, z 1.7. The axis points to the driver (-z) and up by TILT_DEG; the column
# leaves the dashboard (in front, +z, and lower) at the end of -Z.
t = math.radians(TILT_DEG)
zax = (0.0, math.sin(t), -math.cos(t))
yax = (0.0, math.cos(t), math.sin(t))       # the wheel's 12 o'clock leans away from the driver
xax = cross(yax, zax)                       # X = Y x Z (= -x of the RV: the frame is right-handed)
origin = (0.45, 1.58, 2.28)   # low enough that its top rim stays under the driver's line of sight to the gauges
json.dump({'comment': "Where steering_wheel.obj goes in rv.obj's frame (meters). The model's own axes x, y, z "
                      "map to the world vectors below; its origin to `origin`. Turn the wheel about its z.",
           'steering_wheel': {'model': 'steering_wheel.obj', 'origin': origin, 'x': xax, 'y': yax, 'z': zax,
                              'column_end': [origin[i] - zax[i]*COLUMN_LEN for i in range(3)]}},
          open('steering_wheel_mounts.json', 'w'), indent=2)
