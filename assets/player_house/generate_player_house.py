"""Generates the player_house map's models: the flat desert floor and a traditional well.

- house_floor.obj/.mtl + house_sand.jpg: SIZE x SIZE metres centred on the origin (1 m grid, y up),
  mostly flat with gentle Perlin swells (within about +-1.5 m, levelled at the start and the well;
  the engine reads it as the height field); the
  sand texture repeats every TILE metres.
- well.obj/.mtl (+ well_stone.jpg, well_wood.jpg): a traditional well standing on the origin, faceted
  (no `vn`: each face has its own vertices, the engine makes flat normals): a round drystone
  curb 1 m wide and 0.9 m high with dark water inside, two wooden posts with a cross roller and a
  crank, a rope with a bucket and a small gabled roof. About 2.8 m tall; the roller runs along x.

Meters, Y up, like the rest of the project. Needs numpy and Pillow. Fixed seed.
"""
import math

import numpy as np
from PIL import Image

rng = np.random.default_rng(77)

SIZE = 200.0
AMPLITUDE = 3.0  # metres per unit of noise (the result stays within about +-1.5 m)
TILE = 6.0


class Perlin:
    """Classic 2D gradient (Perlin) noise, about -0.7..0.7, seeded."""

    def __init__(self, seed):
        r = np.random.default_rng(seed)
        self.perm = np.concatenate([r.permutation(256)] * 2)
        ang = r.random(256) * 2 * math.pi
        self.grad = np.stack([np.cos(ang), np.sin(ang)], 1)

    def __call__(self, x, y):
        xi, yi = math.floor(x), math.floor(y)
        xf, yf = x - xi, y - yi
        fade = lambda t: t * t * t * (t * (t * 6 - 15) + 10)
        u, v = fade(xf), fade(yf)

        def dot(ix, iy, dx, dy):
            g = self.grad[self.perm[self.perm[ix & 255] + (iy & 255)]]
            return g[0] * dx + g[1] * dy
        n00 = dot(xi, yi, xf, yf)
        n10 = dot(xi + 1, yi, xf - 1, yf)
        n01 = dot(xi, yi + 1, xf, yf - 1)
        n11 = dot(xi + 1, yi + 1, xf - 1, yf - 1)
        return (n00 * (1 - u) + n10 * u) * (1 - v) + (n01 * (1 - u) + n11 * u) * v


PERLIN = [Perlin(1010), Perlin(2020), Perlin(3030)]
WELL = (-14.0, 11.0)  # PlayerHouseStage::WELL_X/Z


def smooth(a, b, x):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def floor_height(x, z):
    """Mostly flat desert: fractal Perlin noise (broad swells 110 m, 45 m and 18 m wide), whose
    height is itself modulated by a very broad noise so some stretches are nearly dead flat; the
    start (the RV) and the well are levelled smoothly."""
    swell = 1.0 * PERLIN[0](x / 110.0, z / 110.0) + 0.4 * PERLIN[1](x / 45.0, z / 45.0) + \
        0.12 * PERLIN[2](x / 18.0, z / 18.0)
    flat = 0.3 + 0.7 * smooth(-0.25, 0.25, PERLIN[1](x / 160.0 + 5.0, z / 160.0 - 3.0))
    near = min(smooth(8.0, 26.0, math.hypot(x, z)),
               smooth(4.0, 14.0, math.hypot(x - WELL[0], z - WELL[1])))
    return AMPLITUDE * swell * flat * near


def noise(n, cells, rng):
    """Tileable value noise 0..1 (n x n)."""
    g = rng.random((cells, cells))
    t = np.linspace(0, cells, n, endpoint=False)
    i = t.astype(int)
    f = t - i
    f = f * f * (3 - 2 * f)
    i1 = (i + 1) % cells
    a = g[np.ix_(i, i)] * (1 - f)[None, :] + g[np.ix_(i, i1)] * f[None, :]
    b = g[np.ix_(i1, i)] * (1 - f)[None, :] + g[np.ix_(i1, i1)] * f[None, :]
    return a * (1 - f)[:, None] + b * f[:, None]


def save(arr, name):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(name, quality=92)


def make_sand(n=512):
    v = 0.5 * noise(n, 8, rng) + 0.3 * noise(n, 32, rng) + 0.2 * noise(n, 128, rng)
    grain = rng.random((n, n)) * 0.25
    t = np.clip(v * 0.8 + grain * 0.4, 0, 1)
    c0, c1 = np.array([201, 168, 118.0]), np.array([232, 205, 152.0])
    save(c0 + (c1 - c0) * t[..., None], 'house_sand.jpg')


def make_stone(n=256):
    """Rough blocks in courses, with mortar between them."""
    img = np.zeros((n, n, 3))
    mortar = np.array([120, 112, 100.0])
    rows = 4
    h = n // rows
    base = noise(n, 16, rng)
    for r in range(rows):
        cols = 4
        w = n // cols
        off = (w // 2) if r % 2 else 0
        for c in range(cols):
            tone = 0.75 + 0.25 * rng.random()
            col = np.array([176, 160, 134.0]) * tone
            for y in range(r * h, (r + 1) * h):
                for x in range(c * w, (c + 1) * w):
                    xx = (x + off) % n
                    edge = min(y - r * h, (r + 1) * h - 1 - y, x - c * w, (c + 1) * w - 1 - x)
                    img[y, xx] = mortar if edge < 4 else col
    img *= (0.85 + 0.3 * base)[..., None]
    save(img, 'well_stone.jpg')


def make_wood(n=256):
    """Weathered planks: grain along u (the long way: v = along the piece)."""
    stripes = noise(n, 24, rng)[:, :1] * np.ones((1, n))
    fine = rng.random((n, n)) * 0.15
    t = np.clip(0.7 * stripes + fine, 0, 1)
    c0, c1 = np.array([92, 66, 44.0]), np.array([150, 112, 76.0])
    save(c0 + (c1 - c0) * t[..., None], 'well_wood.jpg')


class Obj:
    """Faceted geometry: every face has its own vertices."""

    def __init__(self):
        self.faces = []  # (mat, [(v, uv)...])

    def tri(self, mat, pts, uvs, outward=None):
        a, b, c = [np.array(p, float) for p in pts]
        n = np.cross(b - a, c - a)
        if outward is not None and np.dot(n, outward) < 0:
            pts, uvs = pts[::-1], uvs[::-1]
        self.faces.append((mat, list(zip(pts, uvs))))

    def quad(self, mat, pts, uvs, outward=None):
        """Two triangles, wound so that the normal agrees with `outward` (if given)."""
        if outward is None:
            c = np.mean(pts, 0)
            outward = np.cross(np.array(pts[1]) - pts[0], np.array(pts[2]) - pts[0])
        self.tri(mat, [pts[0], pts[1], pts[2]], [uvs[0], uvs[1], uvs[2]], outward)
        self.tri(mat, [pts[0], pts[2], pts[3]], [uvs[0], uvs[2], uvs[3]], outward)

    def box(self, mat, centre, size, rot=None):
        """Axis box `size` = (x, y, z) at `centre`, turned by the 3x3 matrix `rot` about its centre."""
        r = np.eye(3) if rot is None else rot
        hx, hy, hz = [s / 2 for s in size]
        c = np.array(centre, float)

        def p(x, y, z):
            return tuple(c + r @ np.array([x * hx, y * hy, z * hz]))
        sides = [((1, 0, 0), [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),
                 ((-1, 0, 0), [(-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1)]),
                 ((0, 1, 0), [(-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1)]),
                 ((0, -1, 0), [(-1, -1, 1), (-1, -1, -1), (1, -1, -1), (1, -1, 1)]),
                 ((0, 0, 1), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
                 ((0, 0, -1), [(1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)])]
        for nrm, corners in sides:
            along = max(size)
            uvs = [(0, 0), (1, 0), (1, along), (0, along)]
            self.quad(mat, [p(*q) for q in corners], uvs, r @ np.array(nrm, float))

    def cylinder(self, mat, base, axis, radius, length, sides=12, caps=True, radius_top=None):
        """Cylinder (or cone frustum) from `base` along unit `axis`."""
        axis = np.array(axis, float)
        axis /= np.linalg.norm(axis)
        up = np.array([0, 1, 0.0]) if abs(axis[1]) < 0.9 else np.array([1, 0, 0.0])
        u = np.cross(axis, up)
        u /= np.linalg.norm(u)
        v = np.cross(axis, u)
        base = np.array(base, float)
        top = base + axis * length
        rt = radius if radius_top is None else radius_top
        ring = lambda k, rad, o: o + rad * (math.cos(2 * math.pi * k / sides) * u + math.sin(2 * math.pi * k / sides) * v)
        for k in range(sides):
            a0, a1 = ring(k, radius, base), ring(k + 1, radius, base)
            b0, b1 = ring(k, rt, top), ring(k + 1, rt, top)
            mid = ring(k + 0.5, 1.0, np.zeros(3))
            self.quad(mat, [tuple(a0), tuple(a1), tuple(b1), tuple(b0)],
                      [(k / sides, 0), ((k + 1) / sides, 0), ((k + 1) / sides, length), (k / sides, length)], mid)
            if caps:
                self.tri(mat, [tuple(base), tuple(a1), tuple(a0)], [(0, 0), (1, 0), (0, 1)], -axis)
                self.tri(mat, [tuple(top), tuple(b0), tuple(b1)], [(0, 0), (1, 0), (0, 1)], axis)

    def write(self, name, mtl):
        with open(name, 'w') as f:
            f.write(f'# generated by generate_player_house.py\nmtllib {mtl}\no {name[:-4]}\n')
            vi = 0
            lines, last = [], None
            for mat, verts in self.faces:
                if mat != last:
                    lines.append(f'usemtl {mat}\n')
                    last = mat
                for p, uv in verts:
                    f.write('v %.4f %.4f %.4f\n' % tuple(p))
                for p, uv in verts:
                    f.write('vt %.4f %.4f\n' % tuple(uv))
                idx = [vi + i + 1 for i in range(3)]
                lines.append('f ' + ' '.join(f'{i}/{i}' for i in idx) + '\n')
                vi += 3
                # (vertices of a face are written right before it: keep file order simple)
                f.write(lines.pop(0) if lines[0].startswith('usemtl') else '')
                f.write(lines.pop(0))
        print(name, len(self.faces), 'triangles')


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def write_well():
    o = Obj()
    SIDES = 16
    R_OUT, R_IN, H = 1.0, 0.68, 0.9
    WATER = 0.42
    # the curb: drystone, each side a block a little off in radius and height
    jit = [rng.uniform(-0.03, 0.03) for _ in range(SIDES)]
    hj = [rng.uniform(-0.025, 0.025) for _ in range(SIDES)]
    for k in range(SIDES):
        a0, a1 = 2 * math.pi * k / SIDES, 2 * math.pi * (k + 1) / SIDES
        ro0, ro1 = R_OUT + jit[k], R_OUT + jit[(k + 1) % SIDES]
        h = H + hj[k]
        h1 = H + hj[(k + 1) % SIDES]
        P = lambda r, a, y: (r * math.cos(a), y, r * math.sin(a))
        mid = np.array([math.cos((a0 + a1) / 2), 0, math.sin((a0 + a1) / 2)])
        # outer wall (two courses of stones: the texture repeats up it)
        o.quad('stone', [P(ro0, a0, 0), P(ro1, a1, 0), P(ro1, a1, h1), P(ro0, a0, h)],
               [(k, 0), (k + 1, 0), (k + 1, 1.2), (k, 1.2)], mid)
        # top of the rim
        o.quad('stone', [P(ro0, a0, h), P(ro1, a1, h1), P(R_IN, a1, h1), P(R_IN, a0, h)],
               [(0, 0), (1, 0), (1, 0.4), (0, 0.4)], np.array([0, 1.0, 0]))
        # inner wall down to the water
        o.quad('stone', [P(R_IN, a0, h), P(R_IN, a1, h1), P(R_IN, a1, WATER), P(R_IN, a0, WATER)],
               [(k, 1), (k + 1, 1), (k + 1, 0.2), (k, 0.2)], -mid)
        # the water: a dark disc, in pieces
        o.tri('water', [(0, WATER, 0), P(R_IN, a1, WATER), P(R_IN, a0, WATER)], [(0, 0), (1, 0), (0, 1)],
              np.array([0, 1.0, 0]))
    # a stone slab at the foot (a doorstep of sorts, so the base does not sit on the sand like a cut)
    o.cylinder('stone', (0, -0.05, 0), (0, 1, 0), R_OUT + 0.18, 0.12, sides=SIDES, radius_top=R_OUT + 0.12)

    # the frame: two posts outside the curb, a ridge beam on top
    PX, PZ = 1.12, 0.0
    POST_H = 2.45
    for sx in (-1, 1):
        o.box('wood', (sx * PX, POST_H / 2 - 0.05, PZ), (0.16, POST_H + 0.1, 0.16))
        # braces to the curb side (diagonal struts front and back)
        for sz in (-1, 1):
            o.box('wood', (sx * PX, 0.45 + 0.55, sz * 0.42), (0.09, 1.55, 0.09),
                  rot_x(sz * -0.38))
    # the roller: along x, with the rope wound on it, a crank at +x
    ROLLER_Y = 1.95
    o.cylinder('wood', (-PX - 0.1, ROLLER_Y, 0), (1, 0, 0), 0.085, 2 * PX + 0.2, sides=10)
    o.cylinder('rope', (-0.35, ROLLER_Y, 0), (1, 0, 0), 0.115, 0.7, sides=10)
    # the crank: an arm off the roller's end and a handle
    ex = PX + 0.1 + 0.02
    o.box('wood', (ex + 0.05, ROLLER_Y - 0.17, 0), (0.06, 0.4, 0.06))
    o.cylinder('wood', (ex + 0.02, ROLLER_Y - 0.37, 0), (1, 0, 0), 0.035, 0.22, sides=8)
    # the rope and the bucket
    ROPE_TOP = ROLLER_Y - 0.1
    BUCKET_TOP = 1.28
    o.cylinder('rope', (0, BUCKET_TOP, 0.0), (0, 1, 0), 0.016, ROPE_TOP - BUCKET_TOP, sides=6)
    o.cylinder('wood', (0, BUCKET_TOP - 0.3, 0), (0, 1, 0), 0.14, 0.3, sides=12, caps=False,
               radius_top=0.17)
    o.cylinder('wood', (0, BUCKET_TOP - 0.3, 0), (0, 1, 0), 0.14, 0.02, sides=12)
    # the handle of the bucket: a small bar over its mouth
    o.box('rope', (0, BUCKET_TOP - 0.02, 0), (0.36, 0.025, 0.025))

    # the roof: two sloped planks (gable along x) with a ridge and two gable triangles
    ROOF_Y, RIDGE_Y = POST_H + 0.02, POST_H + 0.5
    half = 0.72
    slope = math.atan2(RIDGE_Y - ROOF_Y, half)
    L = 2 * PX + 0.7
    for sz in (-1, 1):
        cz = sz * (half / 2 + 0.03)
        cy = (ROOF_Y + RIDGE_Y) / 2 + 0.04
        o.box('wood', (0, cy, cz), (L, 0.05, math.hypot(half, RIDGE_Y - ROOF_Y) + 0.15),
              rot_x(sz * slope))
    o.box('wood', (0, RIDGE_Y + 0.05, 0), (L + 0.04, 0.07, 0.1))
    for sx in (-1, 1):
        x = sx * (PX + 0.08)
        o.tri('wood', [(x, ROOF_Y, -half), (x, ROOF_Y, half), (x, RIDGE_Y, 0)], [(0, 0), (1, 0), (0.5, 0.6)],
              np.array([sx, 0, 0.0]))

    o.write('well.obj', 'well.mtl')
    with open('well.mtl', 'w') as f:
        f.write('# generated by generate_player_house.py\n')
        for name, tex, ks in (('stone', 'well_stone.jpg', 0), ('wood', 'well_wood.jpg', 0.05),
                              ('rope', 'well_wood.jpg', 0)):
            f.write(f'newmtl {name}\nNs 10\nKa 1 1 1\nKd 1 1 1\nKs {ks} {ks} {ks}\nillum 1\nmap_Kd {tex}\n\n')
        f.write('newmtl water\nNs 80\nKa 1 1 1\nKd 0.04 0.09 0.12\nKs 0.3 0.3 0.3\nillum 2\n')
    # a rope darker than the planks: its own colour through Kd
    txt = open('well.mtl').read().replace(
        'newmtl rope\nNs 10\nKa 1 1 1\nKd 1 1 1', 'newmtl rope\nNs 10\nKa 1 1 1\nKd 0.8 0.7 0.55')
    open('well.mtl', 'w').write(txt)


def write_floor():
    n = int(SIZE) + 1
    half = SIZE / 2
    with open('house_floor.obj', 'w') as f:
        f.write('# generated by generate_player_house.py\nmtllib house_floor.mtl\no house_floor\n')
        for j in range(n):
            for i in range(n):
                x, z = -half + i, -half + j
                f.write(f'v {x:.3f} {floor_height(x, z):.4f} {z:.3f}\n')
        for j in range(n):
            for i in range(n):
                f.write(f'vt {i / TILE:.4f} {j / TILE:.4f}\n')
        f.write('usemtl sand\n')
        for j in range(n - 1):
            for i in range(n - 1):
                a = j * n + i + 1
                b, c, d = a + 1, a + n + 1, a + n
                f.write(f'f {a}/{a} {d}/{d} {c}/{c} {b}/{b}\n')
    with open('house_floor.mtl', 'w') as f:
        f.write('# generated by generate_player_house.py\nnewmtl sand\nNs 5\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\n'
                'illum 1\nmap_Kd house_sand.jpg\n')


if __name__ == '__main__':
    make_sand()
    make_stone()
    make_wood()
    write_floor()
    write_well()
