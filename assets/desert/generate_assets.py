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
LOOP_TERRAIN_SIZE = 180.0  # dunes_loop.obj (the day desert) covers [-90, 90]
LOOP_TERRAIN_RES = 180     # quads per side: 1 m each
SAND_TILE = 2.0        # world units covered by one sand.jpg tile
ROAD_WIDTH = 8.0       # wide enough for the RV (2.4 m) with room to spare
ROAD_TILE = 8.0        # world units along the road covered by one road.jpg tile
ROAD_LIFT = 0.07       # the road floats this far above the dunes (no z-fighting)
ROAD_SMOOTH = 8.0      # sigma (world units) of the smoothing of the road's height
ROAD_BLEND = 6.0       # width of the slope that joins the road bed to the dunes
ROAD_STEP = 0.5        # distance between the rows of the road ribbon


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


def fbm(n, rng, first=1.0, last=2.4, octaves=5):
    """Tileable fractal noise: several band-limited layers with falling weight, so there is detail
    at every scale and no single feature size to recognise when the tile repeats."""
    out = np.zeros((n, n))
    for i in range(octaves):
        p = first + (last - first) * i / max(1, octaves - 1)
        out += periodic_noise(n, p, rng) * 0.55 ** i
    return (out - out.min()) / (out.max() - out.min())


def worley(n, cells, rng):
    """Tileable Worley noise: F1, F2 (distance to the nearest and second-nearest feature point, in
    cell units) and the id of the nearest cell. Points wrap around, so the tile is seamless."""
    pts = rng.random((cells, cells, 2))
    ids = rng.random((cells, cells))
    gy, gx = np.mgrid[0:n, 0:n] * (cells / n)
    cy, cx = np.floor(gy).astype(int), np.floor(gx).astype(int)
    f1 = np.full((n, n), 9.0)
    f2 = np.full((n, n), 9.0)
    cell = np.zeros((n, n))
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            iy, ix = cy + dy, cx + dx
            p = pts[iy % cells, ix % cells]
            d = np.hypot(iy + p[..., 0] - gy, ix + p[..., 1] - gx)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            cell = np.where(closer, ids[iy % cells, ix % cells], cell)
            f1 = np.where(closer, d, f1)
    return f1, f2, cell


def make_sand(rng, n=1024):
    """Wind-blown sand. What made the old one look repeated was a single big blotch size and
    parallel, regular ripples. Now: fractal tone at several scales, a diagonal ripple field bent
    by a strong warp and faded in and out by a patch mask (no two stretches alike), a slight
    warm/cool drift, and sparse dark and bright mineral grains."""
    y = np.arange(n)[:, None] / n
    x = np.arange(n)[None, :] / n
    tone = fbm(n, rng, 0.8, 2.2, 6)
    warp = fbm(n, rng, 1.4, 2.4, 4)
    patch = fbm(n, rng, 1.6, 2.4, 3)
    # integer frequencies (9, 5): the ripple tiles but runs at a slant, not along an axis
    ripple = 0.5 + 0.5 * np.sin(2 * np.pi * (9 * x + 5 * y + 1.1 * (warp - 0.5)))
    ripple = ripple * np.clip((patch - 0.25) * 2.5, 0.15, 1)
    micro = fbm(n, rng, 0.2, 1.0, 3)
    t = 0.30 * tone + 0.40 * ripple + 0.30 * micro
    col = lerp_color(t, (190, 146, 92), (236, 205, 148))
    drift = fbm(n, rng, 2.0, 2.6, 2) - 0.5          # warm / cool patches
    col = col + drift[..., None] * np.array([16, 4, -14])
    grain = (periodic_noise(n, 0.0, rng) - 0.5) * 18
    dark = (rng.random((n, n)) > 0.992) * 40.0       # mineral specks
    bright = (rng.random((n, n)) > 0.993) * 38.0     # quartz
    save_jpg(col + (grain - dark + bright)[..., None], "sand.jpg")


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
    """Weathered sandstone: Worley cells are the fractured blocks (each with its own tone), dark
    cracks run where F2 - F1 is small, strata are bent by a strong warp (slanted, integer
    frequencies so it tiles), and ridged fractal noise gives the pitted, eroded surface."""
    y = np.arange(n)[:, None] / n
    x = np.arange(n)[None, :] / n
    f1, f2, cell = worley(n, 7, rng)
    f1b, f2b, _ = worley(n, 19, rng)
    warp = fbm(n, rng, 1.4, 2.4, 4)
    bands = 0.5 + 0.5 * np.sin(2 * np.pi * (3 * x + 8 * y + 4.0 * (warp - 0.5)))
    thin = 0.5 + 0.5 * np.sin(2 * np.pi * (2 * x + 29 * y + 7.0 * (warp - 0.5)))
    ridged = 1 - np.abs(2 * fbm(n, rng, 0.9, 2.0, 5) - 1)
    pits = fbm(n, rng, 0.3, 1.2, 3)
    # cracks are broken up: they only show where a noise mask lets them (no paving look)
    broken = smoothstep(0.45, 0.65, fbm(n, rng, 1.2, 2.2, 4))
    crack = (1 - smoothstep(0.0, 0.035, f2 - f1)) * broken
    crack_small = (1 - smoothstep(0.0, 0.03, f2b - f1b)) * smoothstep(0.6, 0.8, pits)
    t = (0.32 * ridged + 0.26 * bands + 0.10 * thin + 0.17 * pits + 0.10 * (cell - 0.5) + 0.22)
    col = lerp_color(t, (92, 70, 58), (190, 158, 124))
    tint = fbm(n, rng, 2.0, 2.6, 2) - 0.5
    col = col + tint[..., None] * np.array([14, 2, -12])
    col *= (1 - 0.55 * crack - 0.3 * crack_small)[..., None]
    save_jpg(col + (periodic_noise(n, 0.0, rng) - 0.5)[..., None] * 14, "rock.jpg")


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
    mats = {"sand": ("sand_realistic.jpg", (1, 1, 1), 5),
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


def loop_point(t):
    """The closed road: a ring around the starting clearing (t = 0 .. 2 pi). Its
    radius wanders between about 31 and 45 m, so it winds without ever coming
    close to the clearing, nor bending tighter than the RV can turn."""
    r = (38.0 + 5.0 * np.sin(3 * t + 0.7) + 1.2 * np.sin(5 * t + 2.1) +
         2.0 * np.sin(2 * t + 4.0))
    return r * np.cos(t), r * np.sin(t)


class Track:
    """A closed road: its centre line (points every ROAD_STEP metres, going round
    once), its width, and the height of the road bed all along it.

    The road bed is the height of the dunes under the centre line, smoothed
    (circularly, so there is no seam) so that the road ignores the small dunes.
    terrain() then carves the bed into the dunes."""

    def __init__(self, width, point_fn):
        self.width = width
        # dense samples, then points equally spaced along the curve
        t = np.linspace(0.0, 2 * np.pi, 40001)
        px, pz = point_fn(t)
        s = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(px), np.diff(pz)))])
        self.length = s[-1]
        self.n = int(round(self.length / ROAD_STEP))
        self.step = self.length / self.n
        tt = np.interp(np.arange(self.n) * self.step, s, t)
        self.x, self.z = point_fn(tt)
        # tangent and the tightest bend (radius of curvature)
        tx = np.roll(self.x, -1) - np.roll(self.x, 1)
        tz = np.roll(self.z, -1) - np.roll(self.z, 1)
        norm = np.hypot(tx, tz)
        self.tx, self.tz = tx / norm, tz / norm
        turn = np.abs(np.arctan2(np.roll(self.tz, -1) * self.tx - np.roll(self.tx, -1) * self.tz,
                                 np.roll(self.tx, -1) * self.tx + np.roll(self.tz, -1) * self.tz))
        self.min_radius = self.step / turn.max()
        # road bed height: smoothed dune height, with the clearing kept flat
        h = dune_height(self.x, self.z)
        sigma = ROAD_SMOOTH / self.step
        k = np.exp(-0.5 * (np.minimum(np.arange(self.n), self.n - np.arange(self.n)) / sigma) ** 2)
        k /= k.sum()
        h = np.real(np.fft.ifft(np.fft.fft(h) * np.fft.fft(k)))
        self.h = h * smoothstep(5.0, 22.0, np.hypot(self.x, self.z))

    def nearest(self, x, z):
        """For every point of the arrays: the index of the nearest point of the
        centre line, and the distance to it."""
        x = np.asarray(x, dtype=float)
        z = np.asarray(z, dtype=float)
        fx, fz = x.ravel(), z.ravel()
        index = np.empty(len(fx), dtype=int)
        dist = np.empty(len(fx))
        for lo in range(0, len(fx), 4000):
            sl = slice(lo, lo + 4000)
            d2 = (fx[sl, None] - self.x[None, :]) ** 2 + (fz[sl, None] - self.z[None, :]) ** 2
            index[sl] = d2.argmin(axis=1)
            dist[sl] = np.sqrt(d2[np.arange(len(index[sl])), index[sl]])
        return index.reshape(x.shape), dist.reshape(x.shape)

    def terrain(self, x, z):
        """Dunes with the road bed carved/filled in along the road (arrays)."""
        idx, dist = self.nearest(x, z)
        base = dune_height(np.asarray(x, dtype=float), np.asarray(z, dtype=float))
        w = 1.0 - smoothstep(self.width / 2 + 0.5, self.width / 2 + 0.5 + ROAD_BLEND, dist)
        return base * (1 - w) + self.h[idx] * w


def make_dunes(name, size, res, track=None):
    """The dunes (a regular grid, which is what the stage's height field needs),
    with the road bed carved into them if there is a track."""
    n = res
    m = Mesh("sand")
    xs = np.linspace(-size / 2, size / 2, n + 1)
    gx, gz = np.meshgrid(xs, xs)                      # gz[j, i] = z of row j
    heights = track.terrain(gx, gz) if track else dune_height(gx, gz)
    idx = np.zeros((n + 1, n + 1), dtype=int)
    for j, z in enumerate(xs):
        for i, x in enumerate(xs):
            idx[j, i] = m.add_vertex((x, float(heights[j, i]), z),
                                     (x / SAND_TILE, z / SAND_TILE))
    for j in range(n):
        for i in range(n):
            a, b, c, d = idx[j, i], idx[j, i + 1], idx[j + 1, i], idx[j + 1, i + 1]
            m.f.append((a, c, b))     # counter-clockwise seen from above (+y)
            m.f.append((b, c, d))
    m.write(name)


# the numbers a material map is made of (the game's FloorMaterial, see
# src/world/FloorMaterial.h)
MATERIAL_SAND, MATERIAL_ASPHALT = 0, 1
MATERIAL_MAP_STEP = 0.5   # metres per pixel


def make_material_map(track, name, size):
    """A greyscale image of what the floor is made of, for the game's logic (a
    vehicle is slower on sand): each pixel is a material number. It covers the
    floor exactly, [-size/2, size/2] in x and z: the columns go along +x and
    the rows along +z (the first row is the minimum z edge). The asphalt is
    wherever the road ribbon is."""
    n = int(round(size / MATERIAL_MAP_STEP))
    centres = -size / 2 + (np.arange(n) + 0.5) * (size / n)
    gx, gz = np.meshgrid(centres, centres)            # gz[j, i]: row j <-> z
    _, dist = track.nearest(gx, gz)
    pixels = np.where(dist <= track.width / 2, MATERIAL_ASPHALT, MATERIAL_SAND)
    Image.fromarray(pixels.astype(np.uint8), "L").save(os.path.join(OUT, name))
    return float((pixels == MATERIAL_ASPHALT).mean())


def make_road(track, name):
    """A flat-bottomed ribbon laid on the road bed that Track.terrain() makes. It
    goes round the whole loop and closes on itself: the last row is the first
    one again, and the texture repeats a whole number of times so there is no
    seam."""
    m = Mesh("road")
    across = np.linspace(-1, 1, 5)
    repeats = max(1, int(round(track.length / ROAD_TILE)))
    rows = []
    for i in range(track.n + 1):
        k = i % track.n
        nx, nz = -track.tz[k], track.tx[k]            # to the left of the direction of travel
        v = i * track.step * repeats / track.length
        rows.append([m.add_vertex((track.x[k] + a * track.width / 2 * nx,
                                   float(track.h[k]) + ROAD_LIFT,
                                   track.z[k] + a * track.width / 2 * nz),
                                  ((a + 1) / 2, v)) for a in across])
    # wind the triangles so that they face up
    p = [np.array(m.v[rows[0][q]]) for q in (0, 1)] + [np.array(m.v[rows[1][0]])]
    flip = np.cross(p[1] - p[0], p[2] - p[0])[1] < 0
    for k in range(len(rows) - 1):
        for q in range(len(across) - 1):
            a, b = rows[k][q], rows[k][q + 1]
            c, d = rows[k + 1][q], rows[k + 1][q + 1]
            if flip:
                a, b, c, d = b, a, d, c
            m.f.append((a, b, c))
            m.f.append((b, d, c))
    m.write(name)


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
    make_sand(np.random.default_rng(11))
    # (the cactus and road keep the noise they always had: skip what sand and rock used to draw)
    for size in (1024, 1024, 1024):
        rng.standard_normal((size, size))
    make_cactus(rng)
    make_rock(np.random.default_rng(12))
    for size in (512, 512):
        rng.standard_normal((size, size))
    make_road_texture(rng)
    write_mtl()
    # the dunes alone (the night desert), and the day desert's: a road winding
    # round the starting clearing in a closed loop, carved into bigger dunes
    make_dunes("dunes.obj", TERRAIN_SIZE, TERRAIN_RES)
    track = Track(ROAD_WIDTH, loop_point)
    make_dunes("dunes_loop.obj", LOOP_TERRAIN_SIZE, LOOP_TERRAIN_RES, track)
    print("road loop: %.0f m long, %.0f m wide, tightest bend radius %.1f m" %
          (track.length, ROAD_WIDTH, track.min_radius))
    assert track.min_radius > ROAD_WIDTH / 2 + 2, "the road would fold over itself"
    make_cactus_mesh("cactus_a.obj", 1, [(1, 0.9, 0.9), (-1, 1.5, 0.7)])
    make_cactus_mesh("cactus_b.obj", 2, [(-1, 1.1, 1.0)])
    make_rock_mesh("rock_a.obj", 3, 0.6, 0.6)
    make_rock_mesh("rock_b.obj", 4, 0.35, 0.8)
    make_road(track, "road.obj")
    share = make_material_map(track, "dunes_loop_materials.png", LOOP_TERRAIN_SIZE)
    print("material map: %.1f%% of the floor is asphalt" % (100 * share))
    print("done")
