"""Generates seat.obj (+ seat.mtl): a seat for the RV's cab, after seat.webp (a high-back boat seat:
charcoal vinyl with tan panels, a stepped headrest, side bolsters and a black hinge bracket), on a short
black pedestal.

The model is in the SEAT's frame: the origin on the floor under the middle of the cushion, Y up, +Z
the way the person sitting faces (towards the front of the vehicle), +X to his left. RV.cpp places it in
the cab (SEAT_*). Real size: the cushion's top is 0.46 m over the floor, the backrest 0.65 m over the
cushion, 0.52 m wide. One mesh, five materials (charcoal, tan, vinyl, metal, black).

Everything is made of rounded boxes (a box whose edges and corners are rounded to a radius: soft, like
padded vinyl) with their real normals, so that it shades smooth. Closed solids, outward faces.
"""
import math

TEXTURE_SIZE = 0.45  # metres of seat per tile of the wear textures
objs = []  # (material, verts, normals, faces, uvs)


def rot_x(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0], p[1] * c - p[2] * s, p[1] * s + p[2] * c)


def rbox(mat, center, half, r, tilt=0.0, pivot=(0, 0, 0), n_arc=3):
    """A rounded box: `half` sizes, corner radius `r`, centred at `center`, then turned `tilt` radians
    about the x axis through `pivot` (positive: the top goes back, -z)."""
    hx, hy, hz = half
    verts, normals, faces, uvs = [], [], [], []

    def axis_samples(h):
        inner = h - r
        arc = [-h + r * i / n_arc for i in range(n_arc)]
        return arc + [-inner, inner] + [inner + r * (i + 1) / n_arc for i in range(n_arc)]
    for axis in range(3):
        for sign in (-1, 1):
            others = [a for a in range(3) if a != axis]
            su = axis_samples(half[others[0]])
            sv = axis_samples(half[others[1]])
            # drop duplicates produced where the arc meets the flat part
            def clean(vals):
                out = []
                for v in vals:
                    if not out or abs(v - out[-1]) > 1e-9:
                        out.append(v)
                return out
            su, sv = clean(su), clean(sv)
            base = len(verts)
            for u in su:
                for w in sv:
                    p = [0.0, 0.0, 0.0]
                    p[axis] = sign * half[axis]
                    p[others[0]] = u
                    p[others[1]] = w
                    q = [max(-(half[i] - r), min(half[i] - r, p[i])) for i in range(3)]
                    d = [p[i] - q[i] for i in range(3)]
                    l = math.sqrt(sum(x * x for x in d)) or 1.0
                    n = [x / l for x in d]
                    v = [q[i] + n[i] * r + center[i] for i in range(3)]
                    verts.append(tuple(v))
                    normals.append(tuple(n))
                    # box mapping: the plane of the grid's face, TEXTURE_SIZE metres a tile
                    # (the box's own frame, so a turned backrest keeps its texture on it)
                    uvs.append(((p[others[0]] + center[others[0]] + 7 * axis) / TEXTURE_SIZE,
                                (p[others[1]] + center[others[1]] + 3 * sign) / TEXTURE_SIZE))
            nu, nv = len(su), len(sv)
            for i in range(nu - 1):
                for j in range(nv - 1):
                    a, b = base + i * nv + j, base + i * nv + j + 1
                    c, d2 = base + (i + 1) * nv + j + 1, base + (i + 1) * nv + j
                    # outward test with the face normal
                    pa, pb, pc = verts[a], verts[b], verts[c]
                    e1 = [pb[k] - pa[k] for k in range(3)]
                    e2 = [pc[k] - pa[k] for k in range(3)]
                    cr = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
                    nn = normals[a]
                    if sum(cr[k] * nn[k] for k in range(3)) < 0:
                        a, b, c, d2 = a, d2, c, b
                    faces.append([a, b, c, d2])
    if tilt:
        verts = [tuple(pivot[k] + rot_x(tuple(v[k] - pivot[k] for k in range(3)), -tilt)[k] for k in range(3)) for v in verts]
        normals = [rot_x(n, -tilt) for n in normals]
    objs.append((mat, verts, normals, faces, uvs))


TILT = math.radians(12)           # how far the backrest leans back
HINGE = (0.0, 0.44, -0.22)        # where the backrest turns (back of the cushion)
W = 0.26                          # half the width

# pedestal and bracket (black)
rbox('black', (0, 0.015, 0), (0.15, 0.015, 0.17), 0.012)                # floor plate
rbox('black', (0, 0.15, -0.02), (0.045, 0.14, 0.06), 0.02)              # column
rbox('black', (0, 0.285, 0), (0.17, 0.02, 0.2), 0.015)                  # cushion pan
for sx in (-1, 1):                                                       # hinge brackets
    rbox('black', (sx * (W + 0.005), 0.43, -0.215), (0.012, 0.075, 0.04), 0.008, tilt=TILT * 0.5, pivot=HINGE)
    rbox('metal', (sx * (W + 0.018), 0.44, -0.22), (0.008, 0.02, 0.02), 0.006)   # the pivot bolt

# cushion: charcoal body, tan top and front, dark vinyl side panels
rbox('charcoal', (0, 0.375, 0.0), (W, 0.085, 0.27), 0.05)
rbox('tan', (0, 0.447, 0.03), (0.17, 0.016, 0.2), 0.014)                 # tan seat panel
rbox('tan', (0, 0.35, 0.265), (0.2, 0.065, 0.016), 0.014)                # tan front face
for sx in (-1, 1):
    rbox('vinyl', (sx * 0.205, 0.452, 0.05), (0.045, 0.012, 0.17), 0.01)  # side panels, leather-grain
    rbox('charcoal', (sx * 0.2, 0.42, 0.0), (0.06, 0.04, 0.265), 0.035)   # the side bolsters

# backrest: charcoal shell, tan centre panel, tan headrest, vinyl side panels
BACK_Y = 0.44 + 0.31
rbox('charcoal', (0, BACK_Y, HINGE[2] - 0.02), (W, 0.31, 0.06), 0.055, tilt=TILT, pivot=HINGE)
rbox('tan', (0, BACK_Y - 0.06, HINGE[2] + 0.046), (0.155, 0.17, 0.014), 0.012, tilt=TILT, pivot=HINGE)
rbox('tan', (0, BACK_Y + 0.23, HINGE[2] + 0.02), (0.17, 0.075, 0.03), 0.02, tilt=TILT, pivot=HINGE)
for sx in (-1, 1):
    rbox('vinyl', (sx * 0.215, BACK_Y - 0.05, HINGE[2] + 0.045), (0.035, 0.17, 0.012), 0.01, tilt=TILT, pivot=HINGE)
    rbox('charcoal', (sx * 0.22, BACK_Y - 0.06, HINGE[2] + 0.02), (0.05, 0.22, 0.05), 0.04, tilt=TILT, pivot=HINGE)  # bolsters


MATERIALS = {
    'charcoal': (0.30, 0.30, 0.32),
    'tan': (0.75, 0.64, 0.46),
    'vinyl': (0.22, 0.22, 0.23),
    'metal': (0.45, 0.45, 0.47),
    'black': (0.05, 0.05, 0.055),
}


# ---------------------------------------------------------------- the worn textures
# seat_charcoal.png, seat_tan.png, seat_vinyl.png: seamless 512 x 512 tiles of vinyl with its
# grain and its years of use: pale scuffs and scratches, darker dirt in the pores, cracks across the
# folds, patches rubbed shiny or faded, stains, and a few cigarette-sized burns. Fixed seed.
import numpy as np
from PIL import Image

SIZE = 512
WEAR = False   # True: textured with the worn vinyl below (tried, not liked: plain colours for now)
TEXTURED = ('charcoal', 'tan', 'vinyl') if WEAR else ()


def noise(rng, cells, size=SIZE):
    """Smooth seamless value noise (0..1) with `cells` cells across."""
    g = rng.random((cells, cells))
    t = np.linspace(0, cells, size, endpoint=False)
    i0 = t.astype(int)
    f = t - i0
    f = f * f * (3 - 2 * f)
    i1 = (i0 + 1) % cells
    a = g[np.ix_(i0, i0)] * (1 - f)[None, :] + g[np.ix_(i0, i1)] * f[None, :]
    b = g[np.ix_(i1, i0)] * (1 - f)[None, :] + g[np.ix_(i1, i1)] * f[None, :]
    return a * (1 - f)[:, None] + b * f[:, None]


def fbm(rng, base=4, octaves=5):
    out, amp, tot = np.zeros((SIZE, SIZE)), 1.0, 0.0
    for o in range(octaves):
        out += amp * noise(rng, base * 2 ** o)
        tot += amp
        amp *= 0.5
    return out / tot


def stroke(img, rng, x, y, ang, length, width, value, bend=0.0):
    """Paints a wandering line (wrapping at the edges, so the tile stays seamless)."""
    for k in range(int(length)):
        x += math.cos(ang)
        y += math.sin(ang)
        ang += rng.normal(0, 0.05) + bend
        for dx in range(-width, width + 1):
            for dy in range(-width, width + 1):
                if dx * dx + dy * dy <= width * width:
                    img[int(y + dy) % SIZE, int(x + dx) % SIZE] = value


def make_texture(name, base, seed):
    rng = np.random.default_rng(seed)
    rgb = np.ones((SIZE, SIZE, 3)) * np.array(base)
    # the vinyl's grain: a fine pebble relief
    grain = fbm(rng, 128, 2)
    rgb *= (0.88 + 0.24 * grain)[:, :, None]
    # large soft tone changes (sun, hands, sitting)
    rgb *= (0.88 + 0.24 * fbm(rng, 3, 4))[:, :, None]
    # faded and rubbed patches: lighter, greyer
    rub = np.clip((fbm(rng, 3, 5) - 0.52) * 4, 0, 1) ** 1.3
    grey = rgb.mean(axis=2, keepdims=True)
    rgb = rgb * (1 - 0.5 * rub[:, :, None]) + (grey * 1.25) * (0.5 * rub[:, :, None])
    # dirt: dark, in the low places of the grain, and low-frequency stains
    dirt = np.clip((0.5 - grain) * 2, 0, 1) * np.clip(fbm(rng, 6, 4) * 1.6 - 0.4, 0, 1)
    rgb *= (1 - 0.3 * dirt)[:, :, None]
    stain = np.clip((fbm(rng, 2, 6) - 0.62) * 6, 0, 1)
    rgb *= (1 - 0.4 * stain)[:, :, None] * np.array([1.0, 0.93, 0.8])[None, None, :] ** stain[:, :, None]
    # scuffs and scratches (pale lines), cracks (dark thin lines, in clusters), burns (small dark spots)
    scratch = np.zeros((SIZE, SIZE))
    for _ in range(40):
        stroke(scratch, rng, rng.uniform(0, SIZE), rng.uniform(0, SIZE), rng.uniform(0, 6.28),
               rng.uniform(15, 70), 0, 1.0)
    for _ in range(6):   # a few deep ones
        stroke(scratch, rng, rng.uniform(0, SIZE), rng.uniform(0, SIZE), rng.uniform(0, 6.28),
               rng.uniform(60, 160), 1, 1.0)
    crack = np.zeros((SIZE, SIZE))
    for _ in range(5):
        cx, cy, a0 = rng.uniform(0, SIZE), rng.uniform(0, SIZE), rng.uniform(0, 6.28)
        for _ in range(int(rng.integers(5, 12))):   # a cluster of short parallel-ish cracks
            stroke(crack, rng, cx + rng.normal(0, 30), cy + rng.normal(0, 30), a0 + rng.normal(0, 0.25),
                   rng.uniform(20, 60), 0, 1.0, bend=rng.normal(0, 0.004))
    burn = np.zeros((SIZE, SIZE))
    yy, xx = np.mgrid[0:SIZE, 0:SIZE]
    for _ in range(3):
        bx, by, br = rng.uniform(0, SIZE), rng.uniform(0, SIZE), rng.uniform(4, 8)
        d = np.minimum(abs(xx - bx), SIZE - abs(xx - bx)) ** 2 + np.minimum(abs(yy - by), SIZE - abs(yy - by)) ** 2
        burn = np.maximum(burn, np.clip(1 - d / (br * br), 0, 1))
    rgb = rgb * (1 - 0.65 * crack)[:, :, None]
    rgb = rgb * (1 - scratch)[:, :, None] + np.clip(rgb * 1.0 + 0.28, 0, 1) * scratch[:, :, None]
    rgb = rgb * (1 - 0.8 * burn)[:, :, None]
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).save(f'seat_{name}.png')


for i, name in enumerate(TEXTURED):  # (nothing while WEAR is off)
    make_texture(name, MATERIALS[name], 100 + i)

with open('seat.mtl', 'w') as f:
    f.write('# generated by generate_seat.py\n')
    for name, (r, g, b) in MATERIALS.items():
        if name in TEXTURED:   # the colour is in the (worn) texture
            f.write(f'newmtl {name}\nKa 1 1 1\nKd 1 1 1\nKs 0.12 0.12 0.12\nNs 20\nillum 2\nmap_Kd seat_{name}.png\n\n')
        else:
            f.write(f'newmtl {name}\nKa {r:.3f} {g:.3f} {b:.3f}\nKd {r:.3f} {g:.3f} {b:.3f}\nKs 0.12 0.12 0.12\nNs 20\nillum 2\n\n')

with open('seat.obj', 'w') as f:
    f.write('# generated by generate_seat.py\nmtllib seat.mtl\no seat\n')
    offset = 0
    tris = 0
    for mat, verts, normals, faces, uvs in objs:
        f.write(f'usemtl {mat}\n')
        for v in verts:
            f.write('v %.5f %.5f %.5f\n' % v)
        for n in normals:
            f.write('vn %.4f %.4f %.4f\n' % n)
        for t in uvs:
            f.write('vt %.4f %.4f\n' % t)
        for q in faces:
            f.write('f ' + ' '.join(f'{offset + i + 1}/{offset + i + 1}/{offset + i + 1}' for i in q) + '\n')
            tris += 2
        offset += len(verts)
    print('seat.obj:', offset, 'vertices,', tris, 'triangles')
