#!/usr/bin/env python3
"""Procedurally generates the Route 66 map (OBJ + MTL + textures + data files).

Usage: python3 generate_route66.py        (writes next to this script)
Needs numpy and Pillow. Everything is seeded, so the output is reproducible.

What it makes:
  r66_floor.obj          the terrain: a regular grid (the stage's height field), a high-desert
                         plain with the road bed carved into it; flat 200 m around the station
  r66_floor_materials.png  what the floor is made of (1 = asphalt, 0 = sand)
  road.obj               the two-lane road ribbon (double yellow line, faded white edges)
  apron.obj              the station's concrete forecourt and the driveways
  route66_path.txt       the road's centre line: "x z height tx tz", every 10 m
  station.obj            the gas station: diner-style building, canopy, 4 pumps, sign pylon
  pump.obj               one pump (also placed by the stage as the usable ones)
  pole.obj               a telephone pole (wood, crossarm, insulators)
  shield.obj             the Route 66 shield on a post, and billboard.obj a roadside billboard
  far_ground.obj         the desert plane beyond the terrain's edge
  mesa.obj               a flat-topped red rock mesa (placed far from the road)
  stage_constants.txt    where the station is (the stage reads it)

The station is at the middle of the road (STATION_Z), on its right-hand side (-x when driving
towards +z), joined to the road by two driveways. Measures are shared with
src/world/Route66Stage.cpp through stage_constants.txt.
"""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))

ROAD_WIDTH = 8.0
ROAD_START_Z = 0.0
ROAD_END_Z = 20000.0       # 20 km of road...
STATION_Z = 10000.0        # ...with the gas station right in the middle
PATH_STEP = 10.0           # metres between the points of the path file
ROAD_LIFT = 0.07
ROAD_BLEND = 6.0
ROAD_TILE = 16.0           # metres of road per texture tile
FLOOR_TILE = 12.0
X0, X1 = -200.0, 200.0
Z0, Z1 = -150.0, ROAD_END_Z + 150.0
CELL = 8.0
MATERIAL_STEP = 1.0
MAT_SAND, MAT_ASPHALT = 0, 1


def smoothstep(a, b, x):
    t = np.clip((np.asarray(x, dtype=float) - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def pnoise(n, power, rng):
    f = np.fft.fft2(rng.standard_normal((n, n)))
    fy = np.fft.fftfreq(n)[:, None]
    fx = np.fft.fftfreq(n)[None, :]
    r = np.sqrt(fx * fx + fy * fy)
    r[0, 0] = 1.0
    f = f / r ** power
    f[0, 0] = 0
    a = np.real(np.fft.ifft2(f))
    return (a - a.min()) / (a.max() - a.min())


def mix(c0, c1, t):
    t = np.clip(t, 0, 1)[..., None]
    return np.array(c0, dtype=float) * (1 - t) + np.array(c1, dtype=float) * t


def save(arr, name, quality=92):
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    if name.endswith(".jpg"):
        img.save(os.path.join(OUT, name), quality=quality)
    else:
        img.save(os.path.join(OUT, name))


# ------------------------------------------------------------------ textures
def make_textures(rng):
    n = 1024
    large, mid, fine = pnoise(n, 1.7, rng), pnoise(n, 1.0, rng), pnoise(n, 0.3, rng)
    col = mix((176, 128, 84), (214, 168, 112), 0.6 * large + 0.4 * mid)       # red-tan desert
    scrub = smoothstep(0.62, 0.8, pnoise(n, 1.3, rng))
    col = col * (1 - 0.55 * scrub[..., None]) + np.array([112, 112, 66]) * 0.55 * scrub[..., None]
    col += (fine[..., None] - 0.5) * 26
    save(col, "r66_ground.jpg")

    n = 512
    u = np.arange(n)[None, :] / n
    v = np.arange(n)[:, None] / n
    noise, fn = pnoise(n, 1.0, rng), pnoise(n, 0.2, rng)
    col = mix((70, 70, 72), (112, 110, 106), 0.55 * noise + 0.45 * fn)         # sun-bleached asphalt
    cracks = smoothstep(0.80, 0.84, pnoise(n, 0.8, rng))
    col *= 1 - 0.35 * cracks[..., None]
    edge = np.maximum(smoothstep(0.80, 1.0, u), smoothstep(0.20, 0.0, u))
    col = col * (1 - 0.6 * edge[..., None]) + np.array([176, 140, 100]) * 0.6 * edge[..., None]
    paint = np.zeros((n, n))
    paint[:, np.abs(u[0] - 0.10) < 0.008] = 0.8                                 # white edge lines
    paint[:, np.abs(u[0] - 0.90) < 0.008] = 0.8
    yellow = np.zeros((n, n))
    yellow[:, np.abs(u[0] - 0.485) < 0.007] = 1.0                               # double yellow
    yellow[:, np.abs(u[0] - 0.515) < 0.007] = 1.0
    wear = 0.45 + 0.55 * pnoise(n, 0.6, rng)
    paint *= wear
    yellow *= wear
    col = col * (1 - paint[..., None]) + np.array([214, 210, 196]) * paint[..., None]
    col = col * (1 - yellow[..., None]) + np.array([222, 178, 40]) * yellow[..., None]
    save(col, "r66_road.jpg")

    # concrete forecourt: slabs with joints and oil stains
    n = 512
    col = mix((150, 148, 142), (178, 176, 168), 0.5 * pnoise(n, 1.0, rng) + 0.5 * pnoise(n, 0.3, rng))
    xs = np.arange(n)
    joint = ((xs % (n // 4)) < 3)
    col[joint[:, None] | joint[None, :]] *= 0.62
    stain = smoothstep(0.7, 0.85, pnoise(n, 1.6, rng))
    col *= 1 - 0.35 * stain[..., None]
    save(col, "r66_concrete.jpg")

    # wood for poles: vertical grain
    n = 256
    g = pnoise(n, 0.3, rng)
    grain = np.tile(pnoise(n, 1.0, rng)[:1, :], (n, 1)) * 0.6 + g * 0.4
    save(mix((66, 48, 34), (104, 78, 52), grain), "r66_wood.jpg")

    # corrugated painted metal wall pattern for the building (red / cream stripes)
    n = 256
    col = np.zeros((n, n, 3))
    col[:] = (233, 224, 196)
    col[:, :, :] *= (0.93 + 0.07 * np.sin(np.arange(n) / n * 2 * np.pi * 32))[None, :, None]
    save(col, "r66_wall.jpg")

    font = lambda s: ImageFont.truetype("/usr/share/fonts/liberation/LiberationSans-Bold.ttf", s)

    # the Route 66 shield: black numerals on a white shield
    s = 256
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pts = [(28, 20), (228, 20), (228, 150), (128, 246), (28, 150)]
    d.polygon(pts, fill=(250, 250, 250, 255), outline=(10, 10, 10, 255))
    d.line(pts + [pts[0]], fill=(10, 10, 10, 255), width=8)
    d.text((128, 40), "ROUTE", font=font(34), fill=(10, 10, 10, 255), anchor="mt")
    d.text((128, 130), "66", font=font(104), fill=(10, 10, 10, 255), anchor="mm")
    img.save(os.path.join(OUT, "r66_shield.png"))

    # the pylon sign of the station
    img = Image.new("RGB", (512, 256), (206, 36, 32))
    d = ImageDraw.Draw(img)
    d.rectangle((10, 10, 501, 245), outline=(250, 240, 210), width=8)
    d.text((256, 78), "GAS", font=font(104), fill=(250, 240, 210), anchor="mm")
    d.text((256, 168), "ROUTE 66 SERVICE", font=font(44), fill=(250, 240, 210), anchor="mm")
    d.text((256, 218), "OPEN 24 HOURS", font=font(26), fill=(250, 214, 80), anchor="mm")
    img.save(os.path.join(OUT, "r66_station_sign.png"))

    # roadside billboard
    img = Image.new("RGB", (512, 256), (30, 90, 150))
    d = ImageDraw.Draw(img)
    d.rectangle((8, 8, 503, 247), outline=(250, 240, 210), width=6)
    d.text((256, 70), "THE MOTHER ROAD", font=font(60), fill=(250, 240, 210), anchor="mm")
    d.text((256, 140), "CHICAGO  ->  SANTA MONICA", font=font(36), fill=(250, 214, 80), anchor="mm")
    d.text((256, 200), "2,448 MILES OF AMERICA", font=font(34), fill=(250, 240, 210), anchor="mm")
    img.save(os.path.join(OUT, "r66_billboard.png"))

    # red rock for the mesas: strata
    n = 512
    rows = np.arange(n)[:, None] / n
    strata = 0.5 + 0.5 * np.sin(rows * 2 * np.pi * 9 + 4 * pnoise(n, 1.5, rng))
    col = mix((120, 52, 34), (178, 92, 58), 0.6 * strata + 0.4 * pnoise(n, 0.8, rng))
    col += (pnoise(n, 0.2, rng)[..., None] - 0.5) * 24
    save(col, "r66_mesa.jpg")


def write_mtl():
    def tex(name, f, ks=0.0):
        return "newmtl %s\nNs 5\nKa 1 1 1\nKd 1 1 1\nKs %g %g %g\nillum 1\nmap_Kd %s\n\n" % (name, ks, ks, ks, f)

    def flat(name, r, g, b, ks=0.05):
        return "newmtl %s\nNs 20\nKa %g %g %g\nKd %g %g %g\nKs %g %g %g\nillum 2\n\n" % (
            name, r, g, b, r, g, b, ks, ks, ks)

    with open(os.path.join(OUT, "route66.mtl"), "w") as fh:
        fh.write("# generated by generate_route66.py\n")
        fh.write(tex("floor", "r66_ground.jpg"))
        fh.write(tex("road", "r66_road.jpg"))
        fh.write(tex("concrete", "r66_concrete.jpg"))
        fh.write(tex("wood", "r66_wood.jpg"))
        fh.write(tex("wall", "r66_wall.jpg"))
        fh.write(tex("mesa", "r66_mesa.jpg"))
        fh.write(tex("shield", "r66_shield.png"))
        fh.write(tex("stationsign", "r66_station_sign.png"))
        fh.write(tex("billboard", "r66_billboard.png"))
        fh.write(flat("red", 0.72, 0.13, 0.11))
        fh.write(flat("cream", 0.93, 0.89, 0.76))
        fh.write(flat("roof", 0.34, 0.12, 0.10))
        fh.write(flat("steel", 0.58, 0.60, 0.62, 0.3))
        fh.write(flat("glass", 0.30, 0.45, 0.50, 0.3))
        fh.write(flat("black", 0.05, 0.05, 0.06))
        fh.write(flat("pumpred", 0.78, 0.10, 0.08, 0.2))
        fh.write(flat("pumpglass", 0.75, 0.82, 0.80, 0.2))
        fh.write(flat("white", 0.92, 0.92, 0.9))
        fh.write(flat("insulator", 0.25, 0.55, 0.52, 0.3))


# --------------------------------------------------------------- the terrain
def meander(z):
    """Lateral position of the road at z: straight at the ends and around the station."""
    z = np.asarray(z, dtype=float)
    k = smoothstep(150.0, 600.0, z - ROAD_START_Z) * smoothstep(150.0, 450.0, np.abs(z - STATION_Z)) \
        * smoothstep(150.0, 600.0, ROAD_END_Z - z)
    return k * (26.0 * np.sin(z / 1400.0) + 10.0 * np.sin(z / 380.0 + 1.0) + 3.0 * np.sin(z / 110.0 + 2.0))


class Hills:
    """Low rolling plain (a sum of plane waves), flat near the ends and the station."""

    def __init__(self, rng, waves=18):
        self.k = []
        for _ in range(waves):
            wl = rng.uniform(120.0, 900.0)
            a = rng.uniform(0, 2 * np.pi)
            self.k.append((2 * np.pi / wl * np.cos(a), 2 * np.pi / wl * np.sin(a),
                           rng.uniform(0, 2 * np.pi), 0.0075 * wl))

    def __call__(self, x, z):
        x, z = np.asarray(x, dtype=float), np.asarray(z, dtype=float)
        h = np.zeros(np.broadcast(x, z).shape)
        for kx, kz, ph, amp in self.k:
            h = h + amp * np.sin(kx * x + kz * z + ph)
        flat = smoothstep(200.0, 500.0, np.abs(z - STATION_Z)) * smoothstep(-60.0, 200.0, z) \
            * smoothstep(-60.0, 200.0, ROAD_END_Z - z)
        edge = 1.0 - smoothstep(110.0, 190.0, np.abs(x))      # level at the edge of the grid, where the far ground starts
        return h * flat * edge


class Road:
    """The road bed height along the road (a function of z), and the lateral distance to it."""

    def __init__(self, hills):
        self.zs = np.arange(Z0, Z1 + 2.0, 2.0)
        self.xs = meander(self.zs)
        h = hills(self.xs, self.zs)
        sigma = 20.0                       # in 2 m steps: 40 m, so slopes are gentle
        half = int(4 * sigma)
        k = np.exp(-0.5 * (np.arange(-half, half + 1) / sigma) ** 2)
        k /= k.sum()
        pad = np.concatenate([np.full(half, h[0]), h, np.full(half, h[-1])])
        self.h = np.convolve(pad, k, mode="valid")
        self.dx = np.gradient(self.xs, 2.0)

    def x(self, z):
        return np.interp(z, self.zs, self.xs)

    def height(self, z):
        return np.interp(z, self.zs, self.h)

    def dist(self, x, z):
        slope = np.interp(z, self.zs, self.dx)
        return np.abs(x - self.x(z)) / np.sqrt(1 + slope ** 2)


def terrain(road, hills, x, z):
    d = road.dist(x, z)
    w = 1.0 - smoothstep(ROAD_WIDTH / 2 + 0.5, ROAD_WIDTH / 2 + 0.5 + ROAD_BLEND, d)
    # the station's pad: the forecourt and the ground round it is level with the road
    return hills(x, z) * (1 - w) + road.height(z) * w


def write_floor(road, hills):
    nx, nz = int(round((X1 - X0) / CELL)), int(round((Z1 - Z0) / CELL))
    xs, zs = X0 + np.arange(nx + 1) * CELL, Z0 + np.arange(nz + 1) * CELL
    gx, gz = np.meshgrid(xs, zs)
    h = terrain(road, hills, gx, gz)
    dhdz, dhdx = np.gradient(h, CELL)
    n = np.stack([-dhdx, np.ones_like(h), -dhdz], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    rows, cols = h.shape
    with open(os.path.join(OUT, "r66_floor.obj"), "w") as fh:
        fh.write("# generated by generate_route66.py\nmtllib route66.mtl\no r66_floor\n")
        for j in range(rows):
            fh.write("".join("v %.1f %.2f %.1f\n" % (xs[i], h[j, i], zs[j]) for i in range(cols)))
        for j in range(rows):
            fh.write("".join("vt %.3f %.3f\n" % (xs[i] / FLOOR_TILE, zs[j] / FLOOR_TILE) for i in range(cols)))
        for j in range(rows):
            fh.write("".join("vn %.2f %.2f %.2f\n" % tuple(n[j, i]) for i in range(cols)))
        fh.write("usemtl floor\ns 1\n")
        for j in range(rows - 1):
            out = []
            for i in range(cols - 1):
                a = j * cols + i + 1
                b, c, d = a + 1, a + cols, a + cols + 1
                out.append("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, c, c, c, b, b, b))
                out.append("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (b, b, b, c, c, c, d, d, d))
            fh.write("".join(out))


# station geometry (centre of the road at STATION_Z is the origin of the local frame; the
# station stands on the -x side, so "back" = -x and the road runs along +z)
APRON_NEAR, APRON_FAR = ROAD_WIDTH / 2 + 0.3, 36.0   # |x| range of the forecourt
APRON_HALF = 48.0                                      # z half-length of the forecourt
DRIVE_WIDTH = 14.0
DRIVE_Z = (-40.0, 40.0)                                # centres of the two driveways
BUILDING = dict(cx=-30.0, cz=0.0, w=22.0, d=11.0, h=4.2)  # w along z, d along x
PUMP_X = -17.0
PUMP_ZS = (-7.5, -2.5, 2.5, 7.5)
CANOPY = dict(cx=-16.0, cz=0.0, w=30.0, d=11.0, y=4.6)
PYLON = dict(x=-6.2, z=-26.0, h=9.0)


def write_road(road):
    across = np.linspace(-1, 1, 5)
    zs = np.arange(ROAD_START_Z, ROAD_END_Z + 0.1, 4.0)
    verts, uvs, faces = [], [], []
    for z in zs:
        x0 = float(road.x(z))
        sl = float(np.interp(z, road.zs, road.dx))
        norm = np.hypot(sl, 1.0)
        tx, tz = sl / norm, 1.0 / norm
        nx, nz = -tz, tx
        for a in across:
            verts.append((x0 + a * ROAD_WIDTH / 2 * nx, float(road.height(z)) + ROAD_LIFT,
                          z + a * ROAD_WIDTH / 2 * nz))
            uvs.append(((a + 1) / 2, (z - ROAD_START_Z) / ROAD_TILE))
    q = len(across)
    for r in range(len(zs) - 1):
        for c in range(q - 1):
            a, b = r * q + c, r * q + c + 1
            cc, d = (r + 1) * q + c, (r + 1) * q + c + 1
            faces += [(a, b, cc), (b, d, cc)]
    v0 = np.array(verts[faces[0][0]])
    nrm = np.cross(np.array(verts[faces[0][1]]) - v0, np.array(verts[faces[0][2]]) - v0)
    if nrm[1] < 0:
        faces = [(a, cc, b) for a, b, cc in faces]
    with open(os.path.join(OUT, "road.obj"), "w") as fh:
        fh.write("# generated by generate_route66.py\nmtllib route66.mtl\no road\n")
        fh.write("".join("v %.3f %.3f %.3f\n" % p for p in verts))
        fh.write("".join("vt %.4f %.4f\n" % t for t in uvs))
        fh.write("vn 0 1 0\nusemtl road\ns 1\n")
        fh.write("".join("f %d/%d/1 %d/%d/1 %d/%d/1\n" % (a + 1, a + 1, b + 1, b + 1, c + 1, c + 1)
                         for a, b, c in faces))


# ---------------------------------------------------------- tiny OBJ writer
class Obj:
    """Accumulates boxes, prisms and quads, each with a material; outward normals."""

    def __init__(self, name):
        self.name, self.v, self.vt, self.vn, self.groups = name, [], [], [], {}

    def _vert(self, p, uv, n):
        self.v.append(p); self.vt.append(uv); self.vn.append(n)
        return len(self.v)

    def quad(self, mat, p0, p1, p2, p3, uvs=((0, 0), (1, 0), (1, 1), (0, 1)), flip=False, facing=None):
        """A quad p0..p3, counter-clockwise seen from its front."""
        p = [np.array(q, dtype=float) for q in (p0, p1, p2, p3)]
        n = np.cross(p[1] - p[0], p[2] - p[0])
        n = n / (np.linalg.norm(n) + 1e-12)
        if facing is not None:        # the side that must be the front, whatever the order given
            flip = bool(np.dot(n, facing) < 0)
        if flip:
            n = -n
            p = p[::-1]
            uvs = tuple(uvs)[::-1]
        ids = [self._vert(tuple(p[i]), uvs[i], tuple(n)) for i in range(4)]
        g = self.groups.setdefault(mat, [])
        g.append((ids[0], ids[1], ids[2]))
        g.append((ids[0], ids[2], ids[3]))

    def box(self, mat, cx, y0, cz, sx, sy, sz, mats=None, yaw=0.0, uvscale=None):
        """Axis-aligned box (rotated `yaw` around y), x/z centred, from y0 up. mats: per side
        {'top','bottom','px','nx','pz','nz'} overrides."""
        mats = mats or {}
        c, s = np.cos(yaw), np.sin(yaw)

        def T(x, y, z):
            return (cx + c * x + s * z, y0 + y, cz - s * x + c * z)

        hx, hz = sx / 2, sz / 2
        faces = {
            "px": ((hx, 0, -hz), (hx, 0, hz), (hx, sy, hz), (hx, sy, -hz)),
            "nx": ((-hx, 0, hz), (-hx, 0, -hz), (-hx, sy, -hz), (-hx, sy, hz)),
            "pz": ((hx, 0, hz), (-hx, 0, hz), (-hx, sy, hz), (hx, sy, hz)),
            "nz": ((-hx, 0, -hz), (hx, 0, -hz), (hx, sy, -hz), (-hx, sy, -hz)),
            "top": ((-hx, sy, -hz), (hx, sy, -hz), (hx, sy, hz), (-hx, sy, hz)),
            "bottom": ((-hx, 0, hz), (hx, 0, hz), (hx, 0, -hz), (-hx, 0, -hz)),
        }
        for k, pts in faces.items():
            m = mats.get(k, mat)
            if uvscale and m in uvscale:
                u, w = uvscale[m]
                uvs = ((0, 0), (u, 0), (u, w), (0, w))
            else:
                uvs = ((0, 0), (1, 0), (1, 1), (0, 1))
            self.quad(m, *[T(*q) for q in pts], uvs=uvs, flip=True)

    def cyl(self, mat, cx, y0, cz, r, h, seg=10, r_top=None, cap=True):
        r_top = r if r_top is None else r_top
        for i in range(seg):
            a0, a1 = 2 * np.pi * i / seg, 2 * np.pi * (i + 1) / seg
            p = [(cx + r * np.cos(a0), y0, cz + r * np.sin(a0)),
                 (cx + r * np.cos(a1), y0, cz + r * np.sin(a1)),
                 (cx + r_top * np.cos(a1), y0 + h, cz + r_top * np.sin(a1)),
                 (cx + r_top * np.cos(a0), y0 + h, cz + r_top * np.sin(a0))]
            self.quad(mat, *p[::-1], uvs=((0, 0), (0, 1), (1, 1), (1, 0)))
            if cap:
                self.quad(mat, (cx, y0 + h, cz), p[3], p[2], p[2], flip=False) if False else None
        if cap:  # top cap as a fan
            g = self.groups.setdefault(mat, [])
            c = self._vert((cx, y0 + h, cz), (0.5, 0.5), (0, 1, 0))
            ring = [self._vert((cx + r_top * np.cos(2 * np.pi * i / seg), y0 + h,
                                cz + r_top * np.sin(2 * np.pi * i / seg)), (0.5, 0.5), (0, 1, 0))
                    for i in range(seg)]
            for i in range(seg):
                g.append((c, ring[(i + 1) % seg], ring[i]))

    def write(self, name=None):
        with open(os.path.join(OUT, name or (self.name + ".obj")), "w") as fh:
            fh.write("# generated by generate_route66.py\nmtllib route66.mtl\no %s\n" % self.name)
            fh.write("".join("v %.4f %.4f %.4f\n" % p for p in self.v))
            fh.write("".join("vt %.4f %.4f\n" % t for t in self.vt))
            fh.write("".join("vn %.4f %.4f %.4f\n" % n for n in self.vn))
            for mat, tris in self.groups.items():
                fh.write("usemtl %s\ns 1\n" % mat)
                fh.write("".join("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c)
                                 for a, b, c in tris))


def add_pump(o, x, z):
    """A 1950s pump: red body, glass globe-less head with a clock face, hose and nozzle."""
    o.box("black", x, 0.0, z, 1.0, 0.18, 0.7)                        # base
    o.box("pumpred", x, 0.18, z, 0.78, 1.15, 0.52)                  # body
    o.box("pumpglass", x, 1.33, z, 0.80, 0.48, 0.54)                # display head
    o.box("white", x, 1.38, z, 0.62, 0.34, 0.56)                    # dial band
    o.box("pumpred", x, 1.81, z, 0.88, 0.08, 0.60)                  # cap
    o.cyl("steel", x + 0.44, 0.9, z + 0.18, 0.04, 0.1, seg=6)       # nozzle holster
    o.box("black", x + 0.42, 0.35, z, 0.05, 0.5, 0.05)              # hose


def make_station():
    s = Obj("station")
    B, C, P = BUILDING, CANOPY, PYLON
    # --- building: cream walls, red band, a flat roof with a parapet, big glass front towards +x
    x0 = B["cx"]
    s.box("cream", x0, 0.0, B["cz"], B["d"], B["h"], B["w"],
          mats={"top": "roof", "px": "cream"}, uvscale={"wall": (8, 2)})
    s.box("red", x0, B["h"] - 0.9, B["cz"], B["d"] + 0.1, 0.9, B["w"] + 0.1,
          mats={"top": "roof", "bottom": "red"})                   # the red band under the roof
    s.box("roof", x0, B["h"], B["cz"], B["d"] + 0.5, 0.35, B["w"] + 0.5)  # roof slab
    fx = x0 + B["d"] / 2 + 0.03                                       # front face (towards the pumps)
    for k in range(4):                                                 # shop windows
        zc = B["cz"] - 7.5 + k * 5.0
        s.quad("glass", (fx, 0.9, zc - 1.9), (fx, 0.9, zc + 1.9), (fx, 3.0, zc + 1.9), (fx, 3.0, zc - 1.9), facing=(1, 0, 0))
        s.box("steel", fx, 0.8, zc, 0.12, 0.12, 4.1)
        s.box("steel", fx, 3.0, zc, 0.12, 0.12, 4.1)
    s.box("black", fx, 0.0, B["cz"], 0.1, 2.3, 1.6)                   # door
    # service bay doors at the back-left (towards -z): two garage doors on the -z face
    for k in range(2):
        xd = x0 - 2.7 + k * 5.4
        s.quad("steel", (xd + 2.0, 0.0, B["cz"] - B["w"] / 2 - 0.03), (xd - 2.0, 0.0, B["cz"] - B["w"] / 2 - 0.03),
               (xd - 2.0, 3.2, B["cz"] - B["w"] / 2 - 0.03), (xd + 2.0, 3.2, B["cz"] - B["w"] / 2 - 0.03),
               uvs=((0, 0), (1, 0), (1, 1), (0, 1)), facing=(0, 0, -1))
    # --- canopy: roof on four posts, red edge band
    s.box("white", C["cx"], C["y"], C["cz"], C["d"], 0.35, C["w"], mats={"nz": "red", "pz": "red",
                                                                       "px": "red", "nx": "red"})
    s.box("red", C["cx"], C["y"] + 0.35, C["cz"], C["d"] + 0.2, 0.25, C["w"] + 0.2)
    for sx in (-1, 1):
        for sz in (-1, 1):
            s.box("steel", C["cx"] + sx * (C["d"] / 2 - 0.8), 0.0, C["cz"] + sz * (C["w"] / 2 - 1.2),
                  0.45, C["y"], 0.45)
    # --- pumps on islands under the canopy
    for z in PUMP_ZS:
        s.box("concrete", PUMP_X, 0.0, z, 1.6, 0.18, 1.6)
    # --- pylon sign: tall post, a big sign board with a lit-look face on both sides
    s.box("steel", P["x"], 0.0, P["z"], 0.35, P["h"], 0.35)
    s.box("steel", P["x"], P["h"] - 5.2, P["z"], 0.5, 0.2, 0.5)
    sy0, sw, sh = P["h"] - 3.9, 4.6, 2.3
    fx = P["x"]
    s.box("black", fx, sy0 - 0.12, P["z"], 0.3, sh + 0.24, sw + 0.24)
    for sign_x, flip in ((fx + 0.17, False), (fx - 0.17, True)):
        a, b = P["z"] - sw / 2, P["z"] + sw / 2
        if not flip:
            s.quad("stationsign", (sign_x, sy0, a), (sign_x, sy0, b), (sign_x, sy0 + sh, b), (sign_x, sy0 + sh, a), facing=(1, 0, 0))
        else:
            s.quad("stationsign", (sign_x, sy0, b), (sign_x, sy0, a), (sign_x, sy0 + sh, a), (sign_x, sy0 + sh, b), facing=(-1, 0, 0))
    s.write()
    p = Obj("pump")
    add_pump(p, 0.0, 0.0)
    p.write()


def make_far_ground():
    """A huge flat plane a hair below the terrain's edge: the desert out to the horizon."""
    o = Obj("far_ground")
    X, Z0f, Z1f, T = 6000.0, Z0 - 6000.0, Z1 + 6000.0, 40.0
    o.quad("floor", (-X, -0.06, Z0f), (-X, -0.06, Z1f), (X, -0.06, Z1f), (X, -0.06, Z0f),
           uvs=((-X / T, Z0f / T), (-X / T, Z1f / T), (X / T, Z1f / T), (X / T, Z0f / T)), facing=(0, 1, 0))
    o.write()


def make_apron():
    """Concrete forecourt next to the road plus two driveways joining it to the road."""
    o = Obj("apron")
    L = 0.08
    def slab(x_a, x_b, z_a, z_b):
        # slabs are flat on y = ROAD_LIFT + L (the ground is level here), facing up
        o.quad("concrete", (x_a, L, z_a), (x_a, L, z_b), (x_b, L, z_b), (x_b, L, z_a),
               uvs=((0, 0), (0, (z_b - z_a) / 8.0), ((x_a - x_b) / 8.0, (z_b - z_a) / 8.0), ((x_a - x_b) / 8.0, 0)),
               flip=True)
    # (vertex order so that the normal points up, checked below)
    slab(-APRON_FAR, -APRON_NEAR - 6.0, -APRON_HALF, APRON_HALF)
    for zc in DRIVE_Z:
        slab(-APRON_NEAR - 6.0, -ROAD_WIDTH / 2 + 0.3, zc - DRIVE_WIDTH / 2, zc + DRIVE_WIDTH / 2)
    # every normal up
    for i, n in enumerate(o.vn):
        o.vn[i] = (0.0, 1.0, 0.0)
    # fix winding: make each triangle face +y
    for mat, tris in o.groups.items():
        fixed = []
        for a, b, c in tris:
            pa, pb, pc = (np.array(o.v[k - 1]) for k in (a, b, c))
            fixed.append((a, b, c) if np.cross(pb - pa, pc - pa)[1] > 0 else (a, c, b))
        o.groups[mat] = fixed
    o.write()


def make_pole():
    o = Obj("pole")
    H = 9.0
    o.cyl("wood", 0, 0, 0, 0.14, H, seg=8, r_top=0.11)
    o.box("wood", 0, H - 0.7, 0, 0.12, 0.14, 2.2, yaw=0.0)
    o.box("wood", 0, H - 1.9, 0, 0.12, 0.12, 1.5)
    for z in (-0.95, -0.4, 0.4, 0.95):
        o.cyl("insulator", 0, H - 0.56, z, 0.045, 0.14, seg=6)
    o.write()


def make_shield():
    o = Obj("shield")
    o.box("steel", 0, 0, 0, 0.1, 2.1, 0.1)
    for sgn, flip in ((1, False), (-1, True)):
        x = 0.07 * sgn
        a, b = (-0.42, 0.42) if not flip else (0.42, -0.42)
        o.quad("shield", (x, 1.35, a), (x, 1.35, b), (x, 2.2, b), (x, 2.2, a), facing=(sgn, 0, 0))
    o.write()


def make_billboard():
    o = Obj("billboard")
    for z in (-3.0, 3.0):
        o.box("wood", 0, 0, z, 0.3, 5.5, 0.3)
    o.box("black", 0, 3.4, 0, 0.3, 3.2, 8.6)
    for sgn, flip in ((1, False), (-1, True)):
        x = 0.17 * sgn
        a, b = (-4.0, 4.0) if not flip else (4.0, -4.0)
        o.quad("billboard", (x, 3.6, a), (x, 3.6, b), (x, 6.4, b), (x, 6.4, a), facing=(sgn, 0, 0))
    o.write()


def make_mesa(rng):
    """A flat-topped red mesa: stepped, strata texture; ~420 m across, 70 m tall (origin at its centre)."""
    o = Obj("mesa")
    seg = 28
    levels = [(0.0, 1.0), (0.45, 0.86), (0.80, 0.78), (1.0, 0.72)]    # (height fraction, radius fraction)
    H, R = 70.0, 210.0
    ang = 2 * np.pi * np.arange(seg) / seg
    wob = 1 + 0.13 * np.sin(3 * ang + 1.0) + 0.08 * np.sin(5 * ang + 2.0) + 0.04 * rng.standard_normal(seg)
    for k in range(len(levels) - 1):
        (h0, r0), (h1, r1) = levels[k], levels[k + 1]
        for i in range(seg):
            j = (i + 1) % seg
            p = [(R * r0 * wob[i] * np.cos(ang[i]), H * h0 - 2.0, R * r0 * wob[i] * np.sin(ang[i])),
                 (R * r0 * wob[j] * np.cos(ang[j]), H * h0 - 2.0, R * r0 * wob[j] * np.sin(ang[j])),
                 (R * r1 * wob[j] * np.cos(ang[j]), H * h1, R * r1 * wob[j] * np.sin(ang[j])),
                 (R * r1 * wob[i] * np.cos(ang[i]), H * h1, R * r1 * wob[i] * np.sin(ang[i]))]
            u0, u1 = i / seg * 6, (i + 1) / seg * 6
            o.quad("mesa", *p[::-1], uvs=((u1, h0 * 3), (u0, h0 * 3), (u0, h1 * 3), (u1, h1 * 3)))
    g = o.groups.setdefault("mesa", [])
    c = o._vert((0, H, 0), (0.5, 0.5), (0, 1, 0))
    r1 = levels[-1][1]
    ring = [o._vert((R * r1 * wob[i] * np.cos(ang[i]), H, R * r1 * wob[i] * np.sin(ang[i])), (0.5, 0.5), (0, 1, 0))
            for i in range(seg)]
    for i in range(seg):
        g.append((c, ring[(i + 1) % seg], ring[i]))
    o.write()


def write_path(road):
    with open(os.path.join(OUT, "route66_path.txt"), "w") as fh:
        fh.write("# x z height tx tz, one point every %g m (generate_route66.py)\n" % PATH_STEP)
        for z in np.arange(ROAD_START_Z, ROAD_END_Z + 0.1, PATH_STEP):
            sl = float(np.interp(z, road.zs, road.dx))
            nrm = np.hypot(sl, 1.0)
            fh.write("%.3f %.3f %.3f %.5f %.5f\n" % (road.x(z), z, road.height(z) + 0.0, sl / nrm, 1 / nrm))


def write_material_map(road):
    w, hgt = int(round((X1 - X0) / MATERIAL_STEP)), int(round((Z1 - Z0) / MATERIAL_STEP))
    cx = X0 + (np.arange(w) + 0.5) * (X1 - X0) / w
    cz = Z0 + (np.arange(hgt) + 0.5) * (Z1 - Z0) / hgt
    gx, gz = np.meshgrid(cx, cz)
    d = road.dist(gx, gz)
    inside = (gz >= ROAD_START_Z) & (gz <= ROAD_END_Z)
    px = np.where((d <= ROAD_WIDTH / 2) & inside, MAT_ASPHALT, MAT_SAND)
    # the forecourt and the driveways (the station is on a straight, centred on x = 0 here)
    lx, lz = gx, gz - STATION_Z
    apron = (lx >= -APRON_FAR) & (lx <= -APRON_NEAR - 6.0) & (np.abs(lz) <= APRON_HALF)
    for zc in DRIVE_Z:
        apron |= (lx >= -APRON_NEAR - 6.0) & (lx <= -ROAD_WIDTH / 2 + 0.3) & (np.abs(lz - zc) <= DRIVE_WIDTH / 2)
    px = np.where(apron, MAT_ASPHALT, px)
    Image.fromarray(px.astype(np.uint8), "L").save(os.path.join(OUT, "r66_floor_materials.png"))


def main():
    rng = np.random.default_rng(66)
    make_textures(rng)
    write_mtl()
    hills = Hills(rng)
    road = Road(hills)
    write_floor(road, hills)
    write_road(road)
    write_material_map(road)
    write_path(road)
    make_station()
    make_apron()
    make_far_ground()
    make_pole()
    make_shield()
    make_billboard()
    make_mesa(rng)
    with open(os.path.join(OUT, "stage_constants.txt"), "w") as fh:
        fh.write("station_z %g\nroad_end_z %g\nroad_width %g\napron_far %g\napron_half %g\n"
                 "pump_x %g\npylon_x %g\npylon_z %g\n"
                 % (STATION_Z, ROAD_END_Z, ROAD_WIDTH, APRON_FAR, APRON_HALF, PUMP_X, PYLON["x"], PYLON["z"]))
        fh.write("pump_zs %s\n" % " ".join("%g" % z for z in PUMP_ZS))
        fh.write("building %g %g %g %g %g\n" % (BUILDING["cx"], BUILDING["cz"], BUILDING["w"], BUILDING["d"], BUILDING["h"]))
        fh.write("canopy %g %g %g %g\n" % (CANOPY["cx"], CANOPY["cz"], CANOPY["w"], CANOPY["d"]))
    print("road %.0f m, grid ok" % (ROAD_END_Z - ROAD_START_Z))


if __name__ == "__main__":
    main()
