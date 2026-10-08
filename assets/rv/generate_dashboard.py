"""Generates dashboard.obj / dashboard.mtl: the dashboard of the RV, a separate model
(for now: rv.obj has a plain box there, see 'dash' in generate_rv.py). Retro 1970s
style like the RV (reference.jpg): a vinyl pad with a sloped face towards the driver,
a control panel on the driver's side (left-hand drive: +x; like rv-dashboard-reference.webp: a
black panel with a big gauge, a digital display, the ignition and switches), a radio and
air vents in the middle, a glovebox on the passenger's side, defroster slots on top.
No steering wheel (or column) yet.

Same frame as rv.obj (Y up, front of the vehicle toward +Z, meters, origin on the ground
between the wheels), so it can be dropped in without moving it: it fits under the
windshield (whose lower edge is at z = 3.51, y = 1.8) between the walls (x = +-1.2).
"""
import math

objs = []       # (material, verts, faces): dashboard.obj
glow_objs = []  # the same shapes for what lights up with the front lights (dashboard_glow.obj)
alarm_objs = [] # the red fire-alarm lamp (dashboard_alarm.obj), blinking while the RV is on fire
target = objs   # where add() puts what it makes


def glow():
    """Use as `with glow():`... (a tiny context switch): what is added goes to the glow model."""
    class Switch:
        def __enter__(self):
            global target
            target = glow_objs
        def __exit__(self, *a):
            global target
            target = objs
    return Switch()


def alarm():
    """Use as `with alarm():`: what is added goes to the alarm lamp model."""
    class Switch:
        def __enter__(self):
            global target
            target = alarm_objs
        def __exit__(self, *a):
            global target
            target = objs
    return Switch()


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
    """The profile (z, y points, counter-clockwise) extruded along x."""
    n = len(prof)
    verts = [(x0, y, z) for z, y in prof] + [(x1, y, z) for z, y in prof]
    faces = [list(range(n)), list(range(2*n-1, n-1, -1))]
    for i in range(n):
        j = (i + 1) % n
        faces.append([i, n+i, n+j, j])
    add(mat, verts, faces)


# ---------------------------------------------------------------------- the body
X = 1.15                       # half width
Z_REAR, Z_FRONT = 2.55, 3.50   # from the driver's side to the windshield's base
Y_KNEE, Y_PAD = 1.05, 1.30     # the knee panel is below Y_PAD, the vinyl pad above
FACE_TOP = (2.78, 1.70)        # the sloped face towards the driver: (z, y) of its top...
FACE_BOT = (Z_REAR, Y_PAD)     # ...and of its bottom

# the vinyl pad: a dark soft top, sloping down towards the driver
prism('vinyl', [(Z_FRONT, Y_PAD), (Z_FRONT, 1.78), (3.20, 1.80), (2.95, 1.77), FACE_TOP, FACE_BOT], -X, X)
# the knee panel under it
prism('panel', [(Z_REAR, Y_KNEE), (Z_FRONT, Y_KNEE), (Z_FRONT, Y_PAD), (Z_REAR, Y_PAD)], -X, X)
# the colour stripes of the RV, along the knee panel
box('orange', -X, X, 1.20, 1.24, Z_REAR - 0.004, Z_REAR)
box('teal', -X, X, 1.24, 1.28, Z_REAR - 0.004, Z_REAR)

# The control panel is on a raised pod on the driver's side (a box on the pad with the
# same sloped face, RAISE higher and SHIFT_Z further forward), so that it shows at the
# bottom of the view with the road above it, instead of hiding below the dashboard.
RAISE, SHIFT_Z = 0.22, 0.035
POD_BOT = (FACE_BOT[0] + SHIFT_Z, FACE_BOT[1] + RAISE)     # the sloped face of the pod: its bottom...
POD_TOP = (FACE_TOP[0] + SHIFT_Z, FACE_TOP[1] + RAISE)     # ...and its top
prism('vinyl', [(POD_BOT[0], 1.45), (3.10, 1.45), (3.10, POD_TOP[1]), POD_TOP, POD_BOT], 0.0, 1.0)

# defroster slots on top, along the windshield
for x0, x1 in ((-1.00, -0.08), (0.08, 1.00)):
    box('slot', x0, x1, 1.78, 1.797, 3.30, 3.40)

# ----------------------------------------------- the sloped face: a frame (a, b, c)
# a along x, b up the face, c out of it (towards the driver). A point of the face
# frame is  O + a*U + b*V + c*N.
fz, fy = FACE_TOP[0] - FACE_BOT[0], FACE_TOP[1] - FACE_BOT[1]
FACE_LEN = math.hypot(fz, fy)
O_FACE = (0.0, FACE_BOT[1], FACE_BOT[0])            # the low face: the radio, the vents, the glovebox
O_POD = (0.0, POD_BOT[1], POD_BOT[0])               # the face of the pod: the control panel
O = O_FACE                                          # the one P() uses now (see use_frame)
U = (1.0, 0.0, 0.0)
V = (0.0, fy / FACE_LEN, fz / FACE_LEN)
N = (0.0, fz / FACE_LEN, -fy / FACE_LEN)


def use_frame(origin):
    """Draw (and place) in the frame of the face with this origin (O_FACE or O_POD)."""
    global O
    O = origin


def P(a, b, c):
    return tuple(O[k] + a*U[k] + b*V[k] + c*N[k] for k in range(3))


def fbox(mat, a0, a1, b0, b1, c0, c1):
    v = [P(a0, b0, c0), P(a1, b0, c0), P(a1, b0, c1), P(a0, b0, c1),
         P(a0, b1, c0), P(a1, b1, c0), P(a1, b1, c1), P(a0, b1, c1)]
    add(mat, v, [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]])


def fcyl(mat, a, b, r, c0, c1, seg=28):
    """A cylinder whose axis is the face's normal."""
    verts, faces = [], []
    for c in (c0, c1):
        for i in range(seg):
            t = 2 * math.pi * i / seg
            verts.append(P(a + r * math.cos(t), b + r * math.sin(t), c))
    faces.append(list(range(seg)))
    faces.append(list(range(2*seg-1, seg-1, -1)))
    for i in range(seg):
        j = (i + 1) % seg
        faces.append([i, seg + i, seg + j, j])
    add(mat, verts, faces)


def fbar(mat, a, b, length, width, angle, c0, c1, start=0.0):
    """A thin bar in the face plane, `length` long from `start` away from (a, b)
    in the direction `angle` (radians, 0 = along +a, counter-clockwise)."""
    d = (math.cos(angle), math.sin(angle))
    p = (-d[1], d[0])
    pts = []
    for s, w in ((start, -width/2), (start + length, -width/2), (start + length, width/2), (start, width/2)):
        pts.append((a + d[0]*s + p[0]*w, b + d[1]*s + p[1]*w))
    v = [P(x, y, c0) for x, y in pts] + [P(x, y, c1) for x, y in pts]
    add(mat, v, [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]])


def fprism(mat, pts, c0, c1):
    """A convex polygon of the face plane, (a, b) points, extruded from c0 to c1."""
    n = len(pts)
    verts = [P(a, b, c0) for a, b in pts] + [P(a, b, c1) for a, b in pts]
    faces = [list(range(n)), list(range(2*n-1, n-1, -1))]
    for i in range(n):
        j = (i + 1) % n
        faces.append([i, n+i, n+j, j])
    add(mat, verts, faces)


def octagon(ca, cb, half, chamfer):
    """A square with its corners cut: the chrome frame of the big gauge."""
    h, c = half, chamfer
    return [(ca + x, cb + y) for x, y in ((h-c, -h), (h, -h+c), (h, h-c), (h-c, h),
                                          (-h+c, h), (-h, h-c), (-h, -h+c), (-h+c, -h))]


def chamfered_rect(ca, cb, half_w, half_h, chamfer):
    """A rectangle with its corners cut: the chrome frame around the gauges."""
    w, h, c = half_w, half_h, chamfer
    return [(ca + x, cb + y) for x, y in ((w-c, -h), (w, -h+c), (w, h-c), (w-c, h),
                                          (-w+c, h), (-w, h-c), (-w, -h+c), (-w+c, -h))]


def gauge(a, b, r, ticks=9, sweep=(-45, 225)):
    """A round gauge: chrome bezel, dark face and ticks (the needle is a model of its own,
    needle.obj). `sweep`: the angles of the first and last tick in the face frame
    (counter-clockwise from +a, which is mirrored for the driver: the open gap is at the bottom)."""
    fcyl('chrome', a, b, r, 0.014, 0.026)
    fcyl('gauge', a, b, r * 0.86, 0.014, 0.030)
    for k in range(ticks):
        ang = math.radians(sweep[0] + (sweep[1] - sweep[0]) * k / (ticks - 1))
        fbar('tick', a, b, r * 0.13, 0.0045, ang, 0.030, 0.033, start=r * 0.62)
        with glow():                         # the same mark, lit
            fbar('glowtick', a, b, r * 0.13, 0.0045, ang, 0.033, 0.0345, start=r * 0.62)
    fcyl('chrome', a, b, 0.0095, 0.033, 0.0405, seg=20)   # the hub over the needle's pivot


# ------------------------------------------- the control panel (like the reference
# photo, rv-dashboard-reference.webp): a black panel set into a cream surround, with
# one big gauge in a chrome frame with its corners cut, a small digital display, the
# ignition switch with its key, two columns of square black switches, and knobs.
use_frame(O_POD)
PA0, PA1, PB0, PB1 = 0.04, 0.96, 0.06, 0.41     # the black panel


def M(a):
    """Mirror along the panel: a is the distance from the panel's +x edge... the driver
    sits on +x and sees +x on the left, so the layout is drawn with a reversed (as
    the photo looks from the driver's seat: display left, switches right)."""
    return PA0 + PA1 - a


def mbox(mat, a0, a1, b0, b1, c0, c1):
    fbox(mat, M(a1), M(a0), b0, b1, c0, c1)


def mfcyl(mat, a, b, r, c0, c1, seg=28):
    fcyl(mat, M(a), b, r, c0, c1, seg)


def mbar(mat, a, b, length, width, angle, c0, c1, start=0.0):
    fbar(mat, M(a), b, length, width, math.pi - angle, c0, c1, start)


def mprism(mat, pts, c0, c1):
    fprism(mat, [(M(a), b) for a, b in reversed(pts)], c0, c1)


def mgauge(a, b, r, ticks=9, sweep=(-45, 225)):
    gauge(M(a), b, r, ticks, sweep)


fbox('cluster', PA0, PA1, PB0, PB1, 0.0, 0.008)
# the cream trim around it, a little raised
for a0, a1, b0, b1 in ((PA0 - 0.02, PA1 + 0.02, PB1, PB1 + 0.02), (PA0 - 0.02, PA1 + 0.02, PB0 - 0.02, PB0),
                       (PA0 - 0.02, PA0, PB0, PB1), (PA1, PA1 + 0.02, PB0, PB1)):
    fbox('panel', a0, a1, b0, b1, 0.0, 0.014)

# the two gauges, speed and fuel, side by side in one chrome frame with its corners cut
GB = 0.235
FRAME_A, FRAME_HW, FRAME_HH = 0.39, 0.205, 0.125
mprism('chrome', chamfered_rect(FRAME_A, GB, FRAME_HW, FRAME_HH, 0.04), 0.008, 0.014)
mprism('cluster', chamfered_rect(FRAME_A, GB, FRAME_HW - 0.016, FRAME_HH - 0.016, 0.03), 0.014, 0.016)
SPEED_A, FUEL_A, GR = 0.30, 0.48, 0.078
SPEED_SWEEP, FUEL_SWEEP = (-45, 225), (30, 150)            # face-frame angles of the first/last tick
mgauge(SPEED_A, GB, GR, ticks=9, sweep=SPEED_SWEEP)        # speedometer: 9 marks, 270 degrees
mgauge(FUEL_A, GB, GR, ticks=5, sweep=FUEL_SWEEP)          # fuel: 5 marks, E ... F, 120 degrees
# (the red mark at the empty end of the fuel gauge)
fbar('redmark', M(FUEL_A), GB, GR * 0.16, 0.007, math.radians(FUEL_SWEEP[0]), 0.030, 0.034, start=GR * 0.62)

# the small digital display, to the left of them: a chrome edge, a green window, digits
mbox('chrome', 0.054, 0.170, 0.140, 0.235, 0.008, 0.014)
mbox('dial', 0.061, 0.163, 0.148, 0.227, 0.014, 0.017)
with glow():                                 # the window, lit (the digits stay dark on it)
    mbox('glowlcd', 0.061, 0.163, 0.148, 0.227, 0.017, 0.0175)
for k in range(4):
    mbox('gauge', 0.068 + 0.023 * k, 0.068 + 0.023 * k + 0.014, 0.160, 0.215, 0.017, 0.019)

# the ignition keyhole, to the right of the gauges and lower: a chrome ring, a black
# keyhole (a round hole with a slot under it)
KA, KB = 0.645, 0.140
mfcyl('chrome', KA, KB, 0.034, 0.008, 0.022)
mfcyl('cluster', KA, KB, 0.026, 0.022, 0.024)
mfcyl('chrome', KA, KB, 0.019, 0.024, 0.027, seg=24)
mfcyl('gauge', KA, KB + 0.006, 0.0075, 0.027, 0.029, seg=14)
mbar('gauge', KA, KB + 0.006, 0.022, 0.0085, math.radians(-90), 0.027, 0.029)

# two columns of square black switches, each with a small lamp on top
for col, a in enumerate((0.735, 0.855)):
    for row, b in enumerate((0.345, 0.265, 0.185)):
        mbox('switch', a - 0.022, a + 0.022, b - 0.022, b + 0.022, 0.008, 0.020)
        mbox('rocker', a - 0.016, a + 0.016, b - 0.016, b + 0.002, 0.020, 0.026)
        mfcyl('lamp_off', a, b + 0.034, 0.0065, 0.008, 0.016, seg=12)
        with glow():                         # the pilot lamp, lit
            mfcyl('glowlamp', a, b + 0.034, 0.0065, 0.016, 0.0175, seg=12)
# the fire alarm: the top lamp of the column on the driver's right (the last one made above) gets a
# red lens, bigger than the lamp and a bit proud of it; RV.cpp shows it only while it blinks
with alarm():
    mfcyl('alarm', 0.855, 0.345 + 0.034, 0.0085, 0.0155, 0.019, seg=16)
# and two round knobs under them (lights, wipers)
for a in (0.735, 0.855):
    mfcyl('chrome', a, 0.105, 0.030, 0.008, 0.018)
    mfcyl('knob', a, 0.105, 0.024, 0.018, 0.034, seg=20)
    mbar('chrome', a, 0.105, 0.020, 0.005, math.radians(90), 0.034, 0.038)

# ---------------------------------------------------------------- the middle stack
use_frame(O_FACE)
# radio: dark body, a dial window, two knobs and five preset buttons
fbox('radio', -0.44, -0.06, 0.20, 0.33, 0.0, 0.016)
fbox('dial', -0.37, -0.13, 0.26, 0.31, 0.016, 0.019)
fbox('needle', -0.27, -0.265, 0.26, 0.31, 0.019, 0.021)
for a in (-0.415, -0.085):
    fcyl('knob', a, 0.265, 0.020, 0.016, 0.038, seg=18)
for k in range(5):
    fbox('chrome', -0.37 + 0.052 * k, -0.37 + 0.052 * k + 0.036, 0.215, 0.245, 0.016, 0.022)
# air vents under it: a dark recess with louvres
fbox('slot', -0.44, -0.06, 0.05, 0.16, 0.0, 0.010)
for k in range(5):
    fbox('chrome', -0.43, -0.07, 0.065 + 0.019 * k, 0.065 + 0.019 * k + 0.007, 0.010, 0.016)

# ------------------------------------------------------- the passenger's glovebox
fbox('panel', -1.08, -0.55, 0.04, 0.34, 0.0, 0.014)
fbox('slot', -1.08, -0.55, 0.337, 0.345, 0.0, 0.016)      # the seam above the door
fbox('chrome', -0.92, -0.72, 0.285, 0.305, 0.014, 0.026)  # its handle
fbox('chrome', -0.80, -0.78, 0.04, 0.07, 0.014, 0.020)    # a latch

# ------------------------------------------------------- where the ignition key goes
# key.obj (generate_key.py) is a model of its own. Its origin is the shoulder of the key,
# +Z is its axis pointing out of the lock (the blade goes towards -Z, into it), +Y the
# width of the blade (teeth on its -Y edge) and +X = Y x Z. In the lock: Z along the
# normal of the panel (out of it), Y up the panel, so X = V x N. The blade goes into the
# keyhole, whose slot is vertical, with its width along Y.
use_frame(O_POD)                       # (all of this is on the control panel, on the pod)
KEY_C = 0.030                          # the shoulder, just above the keyhole's black slot
key_origin = P(M(KA), KB - 0.001, KEY_C)
key_axes = {'x': tuple(V[(k + 1) % 3] * N[(k + 2) % 3] - V[(k + 2) % 3] * N[(k + 1) % 3] for k in range(3)),
            'y': V, 'z': N}
def needle_mount(a, angles):
    """needle.obj on the gauge at `a`: its pivot over the face of the dial (the face is at c =
    0.030, the ticks go to 0.033), the same axes as the key (x = V x N, y = up the panel, z =
    out of it); `angles` = (at the minimum, at the maximum) of the needle's own angle in
    degrees about its +z (0 = pointing up the panel, positive = counter-clockwise seen from
    the driver): the speedometer's 270 degrees start at the lower left, the fuel gauge's
    120 degrees at the upper left (E)."""
    return {'model': 'needle.obj', 'origin': [round(c, 5) for c in P(a, GB, 0.033)],
            'x': [round(c, 5) + 0.0 for c in key_axes['x']], 'y': [round(c, 5) for c in V],
            'z': [round(c, 5) for c in N], 'angle_at_min_deg': angles[0], 'angle_at_max_deg': angles[1]}


mounts = {
    'comment': 'Where other models go on dashboard.obj (same frame as rv.obj, meters). '
               'A model\'s own axes x, y, z map to the world vectors below; its origin to `origin`.',
    'ignition_key': {'model': 'key.obj', 'origin': [round(c, 5) for c in key_origin],
                     'x': [round(c, 5) + 0.0 for c in key_axes['x']],
                     'y': [round(c, 5) for c in key_axes['y']],
                     'z': [round(c, 5) for c in key_axes['z']]},
    'speed_needle': needle_mount(M(SPEED_A), (135.0, -135.0)),
    'fuel_needle': needle_mount(M(FUEL_A), (60.0, -60.0)),
    'dashboard_lights': {'comment': 'where the lights that the glowing parts give off go (RV frame, meters), '
                                    'one on each glowing component, each with its range: dim, orange, short',
                         'lights': [{'position': [round(c, 5) for c in P(M(a), b, c_)], 'range': r_}
                                    for a, b, c_, r_ in ((SPEED_A, GB, 0.050, 0.42),   # the speedometer
                                                         (FUEL_A, GB, 0.050, 0.42),    # the fuel gauge
                                                         (0.112, 0.1875, 0.040, 0.30), # the digital display
                                                         (0.735, 0.265, 0.035, 0.32),  # the pilot lamps, first column
                                                         (0.855, 0.265, 0.035, 0.32))],# ... second column
                         'pod_origin': [round(c, 5) for c in O_POD]},
    'keyhole': {'centre': [round(c, 5) for c in P(M(KA), KB, 0.029)]},
}
import json
with open('dashboard_mounts.json', 'w') as f:
    json.dump(mounts, f, indent=2)
    f.write('\n')

# --------------------------------------------------------------------- the output
MATS = {
    'vinyl': (0.17, 0.12, 0.09), 'panel': (0.80, 0.72, 0.55), 'orange': (1.00, 0.45, 0.05),
    'teal': (0.05, 0.90, 0.65), 'slot': (0.03, 0.03, 0.03), 'cluster': (0.07, 0.06, 0.05),
    'chrome': (0.78, 0.78, 0.74), 'gauge': (0.04, 0.06, 0.07), 'tick': (0.92, 0.92, 0.85),
    'needle': (1.00, 0.45, 0.05), 'radio': (0.10, 0.09, 0.08), 'dial': (0.55, 0.78, 0.70),
    'knob': (0.22, 0.19, 0.15), 'switch': (0.09, 0.09, 0.09), 'rocker': (0.26, 0.26, 0.25),
    'lamp_on': (0.95, 0.55, 0.10), 'lamp_off': (0.30, 0.18, 0.08), 'redmark': (0.88, 0.06, 0.04),
}
with open('dashboard.mtl', 'w') as f:
    f.write('# generated by generate_dashboard.py\n')
    for n, c in MATS.items():
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\n'
                'Ks 0.05 0.05 0.05\nNs 10\nillum 2\n\n')
ALARM_MATS = {'alarm': (1.00, 0.04, 0.02)}
GLOW_MATS = {'glowtick': (1.00, 0.88, 0.55), 'glowlamp': (1.00, 0.58, 0.08), 'glowlcd': (0.50, 1.00, 0.70)}
with open('dashboard_glow.mtl', 'w') as f:
    f.write('# generated by generate_dashboard.py\n')
    for n, c in GLOW_MATS.items():
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\n'
                'Ks 0.0 0.0 0.0\nNs 10\nillum 2\n\n')
with open('dashboard_glow.obj', 'w') as f:
    f.write('# generated by generate_dashboard.py: what lights up with the front lights\n'
            'mtllib dashboard_glow.mtl\no dashboard_glow\n')
    off = 0
    for cnt, (mat, verts, faces) in enumerate(glow_objs):
        f.write(f'g part{cnt}\nusemtl {mat}\n')
        for v in verts:
            f.write('v %.5f %.5f %.5f\n' % v)
        for fc in faces:
            f.write('f ' + ' '.join(str(i + 1 + off) for i in fc) + '\n')
        off += len(verts)
print(len(glow_objs), 'glowing parts')
with open('dashboard_alarm.mtl', 'w') as f:
    f.write('# generated by generate_dashboard.py\n')
    for n, c in ALARM_MATS.items():
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\n'
                'Ks 0.0 0.0 0.0\nNs 10\nillum 2\n\n')
with open('dashboard_alarm.obj', 'w') as f:
    f.write('# generated by generate_dashboard.py: the blinking red fire-alarm lamp\n'
            'mtllib dashboard_alarm.mtl\no dashboard_alarm\n')
    off = 0
    allv = []
    for cnt, (mat, verts, faces) in enumerate(alarm_objs):
        f.write(f'g part{cnt}\nusemtl {mat}\n')
        for v in verts:
            f.write('v %.5f %.5f %.5f\n' % v)
            allv.append(v)
        for fc in faces:
            f.write('f ' + ' '.join(str(i + 1 + off) for i in fc) + '\n')
        off += len(verts)
print('alarm lamp centre (RV frame; ALARM_LAMP in RV.cpp):', [round(sum(v[k] for v in allv) / len(allv), 4) for k in range(3)])
with open('dashboard.obj', 'w') as f:
    f.write('# generated by generate_dashboard.py\nmtllib dashboard.mtl\no dashboard\n')
    off = 0
    for cnt, (mat, verts, faces) in enumerate(objs):
        f.write(f'g part{cnt}\nusemtl {mat}\n')
        for v in verts:
            f.write('v %.5f %.5f %.5f\n' % v)
        for fc in faces:
            f.write('f ' + ' '.join(str(i + 1 + off) for i in fc) + '\n')
        off += len(verts)
print(len(objs), 'parts,', off, 'vertices')
