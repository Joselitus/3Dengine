#!/usr/bin/env python3
"""Procedurally generates the desert assets (OBJ + MTL + JPG textures).

Usage: python3 generate_assets.py        (writes next to this script)
Needs numpy and Pillow. Everything is seeded, so the output is reproducible.
"""
import os
import numpy as np
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))
TERRAIN_SIZE = 160.0   # dunes.obj covers [-80, 80] on x and z
TERRAIN_RES = 192      # quads per side
SAND_TILE = 5.0        # world units covered by one sand.jpg tile
ROAD_WIDTH = 4.0
ROAD_TILE = 8.0        # world units along the road covered by one road.jpg tile
ROAD_LIFT = 0.07       # the road floats this far above the dunes (no z-fighting)


# ------------------------------------------------------------------ textures
def periodic_noise(n, power, rng):
    """Tileable noise: white noise filtered with 1/f^power in Fourier space."""
    f = np.fft.fft2(rng.standard_normal((n, n)))
    fy = np.fft.fftfreq(n)[:, None]
    fx = np.fft.fftfreq(n)[None, :]
    r = np.sqrt(fx * fx + fy * fy)
    r[0, 0] = 1.0
    f = f / r ** power
    f[0, 0] = 0
    a = np.real(np.fft.ifft2(f))
    return (a - a.min()) / (a.max() - a.min())


def lerp_color(t, c0, c1):
    t = np.clip(t, 0, 1)[..., None]
    return np.array(c0) * (1 - t) + np.array(c1) * t


def save_jpg(arr, name):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(
        os.path.join(OUT, name), quality=92)


def make_sand(rng, n=1024):
    large = periodic_noise(n, 1.6, rng)
    fine = periodic_noise(n, 0.4, rng)
    # Wind ripples: sine bands warped by the large noise (integer frequency
    # keeps the texture tileable)
    y = np.arange(n)[:, None] / n
    x = np.arange(n)[None, :] / n
    ripple = 0.5 + 0.5 * np.sin(2 * np.pi * (10 * y + 10 * x * 0.0 + 2.0 * (large - 0.5)))
    t = 0.55 * large + 0.25 * ripple + 0.20 * fine
    col = lerp_color(t, (196, 154, 98), (238, 208, 150))
    grain = (periodic_noise(n, 0.0, rng) - 0.5) * 22
    save_jpg(col + grain[..., None], "sand.jpg")


def make_cactus(rng, n=256):
    u = np.arange(n)[None, :] / n          # around the trunk
    v = np.arange(n)[:, None] / n          # along the trunk
    ribs = 0.5 + 0.5 * np.cos(2 * np.pi * 8 * u + 0 * v)
    noise = periodic_noise(n, 1.2, rng)
    t = 0.55 * ribs + 0.45 * noise
    col = lerp_color(t, (34, 82, 44), (92, 150, 82))
    # Spines: pale dots along the rib crests
    spines = np.zeros((n, n))
    for rib in range(8):
        cx = int((rib + 0.5) / 8 * n - n / 16) % n
        for cy in range(0, n, 12):
            spines[cy % n, cx % n] = 1.0
    spines = np.minimum(1.0, spines + np.roll(spines, 1, 0) + np.roll(spines, 1, 1))
    col = col * (1 - spines[..., None]) + np.array([235, 226, 170]) * spines[..., None]
    save_jpg(col, "cactus.jpg")


def make_rock(rng, n=512):
    big = periodic_noise(n, 1.8, rng)
    fine = periodic_noise(n, 0.8, rng)
    y = np.arange(n)[:, None] / n
    strata = 0.5 + 0.5 * np.sin(2 * np.pi * (6 * y + 1.5 * (big - 0.5)))
    t = 0.45 * big + 0.30 * strata + 0.25 * fine
    col = lerp_color(t, (96, 78, 66), (176, 150, 124))
    save_jpg(col, "rock.jpg")


def make_road_texture(rng, n=512):
    """u runs across the road, v along it: worn asphalt, solid edge lines and a
    dashed centre line."""
    u = np.arange(n)[None, :] / n
    v = np.arange(n)[:, None] / n
    noise = periodic_noise(n, 1.0, rng)
    fine = periodic_noise(n, 0.2, rng)
    t = 0.6 * noise + 0.4 * fine
    col = lerp_color(t, (52, 52, 56), (84, 82, 82))
    # Sand blown in along both edges
    edge = np.maximum(smoothstep(0.80, 1.0, u), smoothstep(0.20, 0.0, u))
    col = col * (1 - 0.55 * edge[..., None]) + np.array([190, 160, 110]) * 0.55 * edge[..., None]
    paint = np.zeros((n, n))
    paint[:, (np.abs(u[0] - 0.08) < 0.012)] = 1.0
    paint[:, (np.abs(u[0] - 0.92) < 0.012)] = 1.0
    dash = ((v[:, 0] % 0.5) < 0.25)[:, None] & (np.abs(u - 0.5) < 0.012)
    paint[dash] = 1.0
    paint *= 0.55 + 0.45 * periodic_noise(n, 0.6, rng)   # chipped paint
    col = col * (1 - paint[..., None]) + np.array([225, 215, 170]) * paint[..., None]
    save_jpg(col, "road.jpg")


# ------------------------------------------------------------------- meshes
class Mesh:
    def __init__(self, material):
        self.material = material
        self.v, self.vt, self.f = [], [], []   # f: triples of vertex indices

    def add_vertex(self, p, uv):
        self.v.append(p)
        self.vt.append(uv)
        return len(self.v) - 1

    def normals(self):
        """Smooth normals; vertices at the same position (UV seams) share them."""
        v = np.array(self.v, dtype=float)
        n = np.zeros_like(v)
        for a, b, c in self.f:
            fn = np.cross(v[b] - v[a], v[c] - v[a])
            n[a] += fn
            n[b] += fn
            n[c] += fn
        keys = {}
        for i, p in enumerate(np.round(v, 5)):
            keys.setdefault(tuple(p), []).append(i)
        for idx in keys.values():
            s = n[idx].sum(axis=0)
            n[idx] = s
        ln = np.linalg.norm(n, axis=1, keepdims=True)
        ln[ln == 0] = 1
        return n / ln

    def write(self, name):
        v = np.array(self.v)
        n = self.normals()
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write("# generated by generate_assets.py\nmtllib desert.mtl\n")
            fh.write("o %s\n" % os.path.splitext(name)[0])
            for p in v:
                fh.write("v %.5f %.5f %.5f\n" % tuple(p))
            for t in self.vt:
                fh.write("vt %.5f %.5f\n" % tuple(t))
            for p in n:
                fh.write("vn %.5f %.5f %.5f\n" % tuple(p))
            fh.write("usemtl %s\ns 1\n" % self.material)
            for a, b, c in self.f:
                fh.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" %
                         (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1,
                          c + 1, c + 1, c + 1))


def write_mtl():
    mats = {"sand": ("sand.jpg", (1, 1, 1), 5),
            "cactus": ("cactus.jpg", (1, 1, 1), 20),
            "rock": ("rock.jpg", (1, 1, 1), 10),
            "road": ("road.jpg", (1, 1, 1), 5)}
    with open(os.path.join(OUT, "desert.mtl"), "w") as fh:
        fh.write("# generated by generate_assets.py\n")
        for name, (tex, kd, ns) in mats.items():
            fh.write("newmtl %s\nNs %d\nKa 1 1 1\nKd %g %g %g\nKs 0 0 0\nillum 1\nmap_Kd %s\n\n"
                     % (name, ns, kd[0], kd[1], kd[2], tex))


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def dune_height(x, z):
    """Height of the dunes (>= 0). Flat in a clearing around the origin so the
    penguin has a place to stand; rises into dunes further away."""
    # Domain warp so the crests meander
    wx = x + 6.0 * np.sin(z * 0.11 + 1.3)
    wz = z + 5.0 * np.sin(x * 0.09)
    d = (np.sin(wx * 0.12 + wz * 0.05) * 0.5 + 0.5) ** 2.0 * 3.2
    d += (np.sin(wz * 0.07 - wx * 0.10 + 2.0) * 0.5 + 0.5) ** 2.5 * 2.2
    d += 0.25 * np.sin(wx * 0.9 + wz * 0.35) * np.sin(wz * 0.6)      # ripples
    d = np.maximum(d, 0)
    r = np.sqrt(x * x + z * z)
    return d * smoothstep(5.0, 22.0, r)


def make_dunes():
    n = TERRAIN_RES
    m = Mesh("sand")
    xs = np.linspace(-TERRAIN_SIZE / 2, TERRAIN_SIZE / 2, n + 1)
    idx = np.zeros((n + 1, n + 1), dtype=int)
    for j, z in enumerate(xs):
        for i, x in enumerate(xs):
            idx[j, i] = m.add_vertex((x, float(terrain_height(x, z)), z),
                                     (x / SAND_TILE, z / SAND_TILE))
    for j in range(n):
        for i in range(n):
            a, b, c, d = idx[j, i], idx[j, i + 1], idx[j + 1, i], idx[j + 1, i + 1]
            m.f.append((a, c, b))     # counter-clockwise seen from above (+y)
            m.f.append((b, c, d))
    m.write("dunes.obj")


def road_center(z):
    """x of the road's centre line at depth z. It passes through the clearing at the
    penguin and meanders away along -z (and +z, behind the camera)."""
    return 5.0 * np.sin(z * 0.09 + 2.7) + 2.5 * np.sin(z * 0.19) - 5.0 * np.sin(2.7)


_ROAD_Z = np.arange(-TERRAIN_SIZE / 2 - 4, TERRAIN_SIZE / 2 + 4, 0.25)
_ROAD_X = road_center(_ROAD_Z)
ROAD_SMOOTH = 8.0      # sigma (world units) of the smoothing of the road's height
ROAD_BLEND = 4.0       # width of the slope that joins the road bed to the dunes


def _road_height_profile():
    """Height of the road bed along the centre line: the dune height under the
    centre line, smoothed so the road ignores the small dunes."""
    h = np.array([dune_height(x, z) for x, z in zip(_ROAD_X, _ROAD_Z)])
    k = np.arange(-4 * ROAD_SMOOTH, 4 * ROAD_SMOOTH + 0.25, 0.25)
    k = np.exp(-0.5 * (k / ROAD_SMOOTH) ** 2)
    k /= k.sum()
    pad = len(k) // 2
    h = np.convolve(np.pad(h, pad, mode="edge"), k, mode="valid")
    # keep the clearing around the origin flat, like the dunes do
    return h * smoothstep(5.0, 22.0, np.hypot(_ROAD_X, _ROAD_Z))


_ROAD_H = _road_height_profile()


def terrain_height(x, z):
    """Dunes with the road bed carved/filled in along the road."""
    h = dune_height(x, z)
    d2 = (_ROAD_X - x) ** 2 + (_ROAD_Z - z) ** 2
    i = int(np.argmin(d2))
    w = 1.0 - smoothstep(ROAD_WIDTH / 2 + 0.5, ROAD_WIDTH / 2 + 0.5 + ROAD_BLEND,
                         np.sqrt(d2[i]))
    return h * (1 - w) + _ROAD_H[i] * w


def make_road():
    """A flat-bottomed ribbon laid on the road bed that terrain_height() makes."""
    m = Mesh("road")
    step = 0.5
    zs = np.arange(-TERRAIN_SIZE / 2 + 1, TERRAIN_SIZE / 2 - 1 + 1e-6, step)
    across = np.linspace(-1, 1, 5)
    rows = []
    length = 0.0
    prev = None
    for z in zs:
        cx = road_center(z)
        if prev is not None:
            length += np.hypot(cx - prev[0], z - prev[1])
        prev = (cx, z)
        dx = road_center(z + 0.01) - road_center(z - 0.01)
        nx, nz = 1.0, -dx / 0.02
        ln = np.hypot(nx, nz)
        nx, nz = nx / ln, nz / ln
        y = float(np.interp(z, _ROAD_Z, _ROAD_H)) + ROAD_LIFT
        row = []
        for a in across:
            row.append(m.add_vertex((cx + a * ROAD_WIDTH / 2 * nx, y,
                                     z + a * ROAD_WIDTH / 2 * nz),
                                    ((a + 1) / 2, length / ROAD_TILE)))
        rows.append(row)
    for k in range(len(rows) - 1):
        for i in range(len(across) - 1):
            a, b = rows[k][i], rows[k][i + 1]
            c, d = rows[k + 1][i], rows[k + 1][i + 1]
            m.f.append((a, b, c))
            m.f.append((b, d, c))
    m.write("road.obj")


def tube(m, path, radius_fn, sides=16, ribs=0, rib_depth=0.0, v_scale=1.0):
    """Sweeps a ribbed circle along `path` (N x 3), closing the end with a dome."""
    path = np.array(path, dtype=float)
    N = len(path)
    tang = np.gradient(path, axis=0)
    tang /= np.linalg.norm(tang, axis=1, keepdims=True)
    up = np.array([0.0, 0.0, 1.0]) if abs(tang[0][2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    side = np.cross(tang[0], up)
    side /= np.linalg.norm(side)
    rings = []
    lengths = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(path, axis=0), axis=1))])
    for k in range(N):
        t = tang[k]
        side = side - t * np.dot(side, t)       # parallel transport
        side /= np.linalg.norm(side)
        other = np.cross(t, side)
        r = radius_fn(k / (N - 1))
        ring = []
        for s in range(sides + 1):
            th = 2 * np.pi * s / sides
            rr = r * (1 + rib_depth * np.cos(ribs * th))
            p = path[k] + rr * (np.cos(th) * side + np.sin(th) * other)
            ring.append(m.add_vertex(tuple(p), (s / sides, lengths[k] * v_scale)))
        rings.append(ring)
    for k in range(N - 1):
        for s in range(sides):
            a, b = rings[k][s], rings[k][s + 1]
            c, d = rings[k + 1][s], rings[k + 1][s + 1]
            m.f.append((a, b, c))
            m.f.append((b, d, c))
    # Dome on the end
    tip = m.add_vertex(tuple(path[-1] + tang[-1] * radius_fn(1.0) * 0.8), (0.5, lengths[-1] * v_scale))
    for s in range(sides):
        m.f.append((rings[-1][s], rings[-1][s + 1], tip))
    return tang


def bezier(p0, p1, p2, p3, n):
    t = np.linspace(0, 1, n)[:, None]
    return ((1 - t) ** 3 * p0 + 3 * (1 - t) ** 2 * t * p1 +
            3 * (1 - t) * t ** 2 * p2 + t ** 3 * p3)


def make_cactus_mesh(name, seed, arms):
    rng = np.random.default_rng(seed)
    m = Mesh("cactus")
    height = 2.4 + 0.4 * rng.random()
    r0 = 0.19
    trunk = np.array([[0, y, 0] for y in np.linspace(-0.05, height, 14)])
    tube(m, trunk, lambda t: r0 * (1 - 0.15 * t ** 3), ribs=8, rib_depth=0.07, v_scale=0.6)
    for (side, y0, up) in arms:
        # Arm leaves the trunk horizontally, then turns up
        start = np.array([side * r0 * 0.5, y0, 0.0])
        out = np.array([side * (0.75 + 0.2 * rng.random()), y0 + 0.05, 0.0])
        top = np.array([out[0] + side * 0.1, y0 + up, 0.0])
        path = bezier(start, start + np.array([side * 0.5, 0, 0]),
                      out + np.array([0, -0.0, 0]) + np.array([side * 0.15, 0.1, 0]),
                      top, 16)
        tube(m, path, lambda t: 0.12 * (1 - 0.15 * t ** 3), sides=12, ribs=6,
             rib_depth=0.07, v_scale=0.6)
    m.write(name)


def make_rock_mesh(name, seed, size, squash):
    rng = np.random.default_rng(seed)
    m = Mesh("rock")
    rings, sides = 18, 28
    phase = rng.random((6, 3)) * 10
    freq = np.array([1.3, 2.1, 3.4, 5.0, 7.0, 9.5])
    amp = np.array([0.30, 0.18, 0.10, 0.05, 0.03, 0.015])

    def lump(p):
        s = 0.0
        for k in range(6):
            s += amp[k] * np.sin(freq[k] * (p[0] * 1.0 + phase[k, 0])) * \
                np.sin(freq[k] * (p[1] * 1.1 + phase[k, 1])) * \
                np.sin(freq[k] * (p[2] * 0.9 + phase[k, 2]))
        return 1 + s

    ids = []
    for j in range(rings + 1):
        phi = np.pi * j / rings
        row = []
        for i in range(sides + 1):
            th = 2 * np.pi * i / sides
            d = np.array([np.sin(phi) * np.cos(th), np.cos(phi), np.sin(phi) * np.sin(th)])
            p = d * lump(d) * size
            p[1] = max(p[1] * squash, -0.25 * size)     # flat-ish bottom
            p[1] += 0.25 * size                         # sit on y = 0
            row.append(m.add_vertex(tuple(p), (i / sides * 2, j / rings * 2)))
        ids.append(row)
    for j in range(rings):
        for i in range(sides):
            a, b = ids[j][i], ids[j][i + 1]
            c, d = ids[j + 1][i], ids[j + 1][i + 1]
            m.f.append((a, b, c))
            m.f.append((b, d, c))
    m.write(name)


if __name__ == "__main__":
    rng = np.random.default_rng(7)
    make_sand(rng)
    make_cactus(rng)
    make_rock(rng)
    make_road_texture(rng)
    write_mtl()
    make_dunes()
    make_cactus_mesh("cactus_a.obj", 1, [(1, 0.9, 0.9), (-1, 1.5, 0.7)])
    make_cactus_mesh("cactus_b.obj", 2, [(-1, 1.1, 1.0)])
    make_rock_mesh("rock_a.obj", 3, 0.6, 0.6)
    make_rock_mesh("rock_b.obj", 4, 0.35, 0.8)
    make_road()
    print("done")
