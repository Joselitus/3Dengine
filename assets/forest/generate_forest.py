#!/usr/bin/env python3
"""Procedurally generates the forest road map (OBJ + MTL + JPG + data files).

Usage: python3 generate_forest.py        (writes next to this script)
Needs numpy and Pillow. Everything is seeded, so the output is reproducible.

What it makes:
  forest_floor.obj      the terrain: a regular grid (the stage's height field) of
                        rolling hills with the road bed carved into it
  forest_floor_materials.png  what the floor is made of (1 = asphalt, 2 = grass)
  road.obj              the road ribbon laid on the bed
  forest_path.txt       the road's centre line: "x z height tx tz", every metre
  ../scenes/forest.scene  the same map described for the web viewer (tools/scene_viewer)
  hero_*.obj            the trees, three levels of detail each (see generate_trees.py)
  hero_trees.txt        every tree of the forest: where it stands, whether it sways in the wind
                        (the ones near the road) and whether its trunk is solid
  *.jpg, forest.mtl     textures and materials of the floor and the road
"""
import os
import numpy as np
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))

import generate_trees   # the detailed trees placed along the road

ROAD_WIDTH = 8.0
ROAD_START_Z = -30.0       # the road begins here (the RV waits near it)
ROAD_END_Z = 3000.0        # ...and ends here: 3 km of road
PATH_STEP = 1.0            # metres between the points of the centre line
ROAD_LIFT = 0.07           # the ribbon floats this far over the bed
ROAD_BLEND = 7.0           # slope that joins the road bed to the hills
ROAD_TILE = 8.0            # metres of road per texture tile
FLOOR_TILE = 6.0           # metres of terrain per texture tile

# The terrain is a regular grid: [X0, X1] x [Z0, Z1], CELL metres per quad
X0, X1 = -148.0, 148.0
Z0, Z1 = -120.0, 3120.0
CELL = 4.0
MATERIAL_STEP = 1.0        # metres per pixel of the material map
MATERIAL_ASPHALT, MATERIAL_GRASS = 1, 2   # FloorMaterial (src/world/FloorMaterial.h)

# Forest layout (distances are from the road's centre line). Every tree is one of the
# detailed ones of generate_trees.py
CLEAR = 5.6                # nothing grows nearer than this
NEAR = 45.0                # the near forest, up to here; beyond it the deep forest (always the
                           # extremely low poly models)...
COLLIDE = 20.0             # ...trunks are solid up to here (beyond, an invisible wall stops you)
ANIMATED = 0.0             # trees nearer than this to the road sway in the wind (0: none, all still)
NEAR_CELL, FAR_CELL = 6.0, 8.5     # metres between trees (jittered grid)
SCENE_ROAD = 500.0         # forest.scene lists the trees of the first stretch of the road only


def smoothstep(a, b, x):
    t = np.clip((np.asarray(x, dtype=float) - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------ textures
def periodic_noise(n, power, rng):
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


def save_jpg(arr, name):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(
        os.path.join(OUT, name), quality=92)


def make_floor_texture(rng, n=1024):
    """Forest floor: moss and grass, dark earth and fallen needles."""
    large = periodic_noise(n, 1.7, rng)
    mid = periodic_noise(n, 1.0, rng)
    fine = periodic_noise(n, 0.3, rng)
    col = mix((38, 62, 28), (74, 98, 44), 0.6 * large + 0.4 * mid)
    earth = smoothstep(0.55, 0.8, periodic_noise(n, 1.4, rng))
    col = col * (1 - 0.7 * earth[..., None]) + np.array([82, 62, 40]) * 0.7 * earth[..., None]
    # needles and twigs
    litter = (fine > 0.74).astype(float)
    col = col * (1 - 0.5 * litter[..., None]) + np.array([112, 84, 52]) * 0.5 * litter[..., None]
    col += (periodic_noise(n, 0.0, rng)[..., None] - 0.5) * 20
    save_jpg(col, "forest_floor.jpg")


def make_road_texture(rng, n=512):
    """u across the road, v along it: old asphalt with moss and dirt at the
    edges, a solid line on each side and a dashed one in the middle."""
    u = np.arange(n)[None, :] / n
    v = np.arange(n)[:, None] / n
    noise = periodic_noise(n, 1.0, rng)
    fine = periodic_noise(n, 0.2, rng)
    col = mix((48, 50, 52), (82, 82, 80), 0.6 * noise + 0.4 * fine)
    edge = np.maximum(smoothstep(0.78, 1.0, u), smoothstep(0.22, 0.0, u))
    moss = periodic_noise(n, 1.2, rng)
    col = col * (1 - 0.7 * edge[..., None]) + mix((60, 78, 40), (96, 84, 56), moss) * 0.7 * edge[..., None]
    paint = np.zeros((n, n))
    paint[:, (np.abs(u[0] - 0.10) < 0.010)] = 1.0
    paint[:, (np.abs(u[0] - 0.90) < 0.010)] = 1.0
    dash = ((v[:, 0] % 0.5) < 0.25)[:, None] & (np.abs(u - 0.5) < 0.012)
    paint[dash] = 1.0
    paint *= 0.45 + 0.55 * periodic_noise(n, 0.6, rng)
    col = col * (1 - paint[..., None]) + np.array([218, 208, 160]) * paint[..., None]
    save_jpg(col, "road.jpg")


def write_mtl():
    mats = [("floor", "forest_floor.jpg"), ("road", "road.jpg")]
    with open(os.path.join(OUT, "forest.mtl"), "w") as fh:
        fh.write("# generated by generate_forest.py\n")
        for name, tex in mats:
            fh.write("newmtl %s\nNs 5\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\nillum 1\nmap_Kd %s\n\n"
                     % (name, tex))


# --------------------------------------------------------------- the terrain
def meander(z):
    """Lateral position of the road at z: straight at the start, then bending
    gently (never tighter than a few hundred metres of radius)."""
    z = np.asarray(z, dtype=float)
    k = smoothstep(0.0, 220.0, z)
    return k * (22.0 * np.sin(z / 310.0) + 12.0 * np.sin(z / 130.0 + 1.0) +
                6.0 * np.sin(z / 55.0 + 2.0))


class Hills:
    """Rolling hills: a sum of plane waves (long ones tall, short ones low)."""

    def __init__(self, rng, waves=16):
        self.k = []
        for _ in range(waves):
            wavelength = rng.uniform(60.0, 420.0)
            angle = rng.uniform(0, 2 * np.pi)
            self.k.append((2 * np.pi / wavelength * np.cos(angle),
                           2 * np.pi / wavelength * np.sin(angle),
                           rng.uniform(0, 2 * np.pi),
                           0.0065 * wavelength))

    def __call__(self, x, z):
        x = np.asarray(x, dtype=float)
        z = np.asarray(z, dtype=float)
        h = np.zeros(np.broadcast(x, z).shape)
        for kx, kz, phase, amp in self.k:
            h = h + amp * np.sin(kx * x + kz * z + phase)
        # flat where the road starts, so the RV and the player stand on level ground
        return h * smoothstep(-60.0, 120.0, z)


class Road:
    """The centre line (a point every PATH_STEP metres along the road), and the
    height of the road bed all along it."""

    def __init__(self, hills):
        zz = np.arange(ROAD_START_Z, ROAD_END_Z, 0.25)
        xx = meander(zz)
        s = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(xx), np.diff(zz)))])
        self.length = s[-1]
        self.n = int(self.length / PATH_STEP)
        along = np.arange(self.n) * PATH_STEP
        self.z = np.interp(along, s, zz)
        self.x = meander(self.z)
        tx = np.gradient(self.x)
        tz = np.gradient(self.z)
        norm = np.hypot(tx, tz)
        self.tx, self.tz = tx / norm, tz / norm
        turn = np.abs(np.arctan2(self.tx[:-1] * self.tz[1:] - self.tz[:-1] * self.tx[1:],
                                 self.tx[:-1] * self.tx[1:] + self.tz[:-1] * self.tz[1:]))
        self.min_radius = PATH_STEP / max(turn.max(), 1e-9)
        # the bed follows the hills, smoothed so the road has gentle slopes
        h = hills(self.x, self.z)
        sigma = 35.0 / PATH_STEP
        half = int(4 * sigma)
        k = np.exp(-0.5 * (np.arange(-half, half + 1) / sigma) ** 2)
        k /= k.sum()
        padded = np.concatenate([np.full(half, h[0]), h, np.full(half, h[-1])])
        self.h = np.convolve(padded, k, mode="valid")

    def nearest(self, x, z):
        """For every point: the index of the nearest centre-line point and the
        distance to it. The road runs along z, so only the points whose z is
        near the point's are tried (a coarse pass, then a fine one)."""
        x = np.asarray(x, dtype=float)
        z = np.asarray(z, dtype=float)
        fx, fz = x.ravel(), z.ravel()
        guess = np.clip(np.round((fz - self.z[0]) / (self.z[-1] - self.z[0]) * (self.n - 1)),
                        0, self.n - 1).astype(int)
        index = np.empty(len(fx), dtype=int)
        dist = np.empty(len(fx))
        for lo in range(0, len(fx), 20000):
            sl = slice(lo, lo + 20000)
            best = np.full(len(fx[sl]), 1e30)
            where = guess[sl].copy()
            for off in range(-180, 181, 3):
                i = np.clip(guess[sl] + off, 0, self.n - 1)
                d2 = (fx[sl] - self.x[i]) ** 2 + (fz[sl] - self.z[i]) ** 2
                better = d2 < best
                best[better] = d2[better]
                where[better] = i[better]
            centre = where.copy()
            for off in range(-3, 4):
                i = np.clip(centre + off, 0, self.n - 1)
                d2 = (fx[sl] - self.x[i]) ** 2 + (fz[sl] - self.z[i]) ** 2
                better = d2 < best
                best[better] = d2[better]
                where[better] = i[better]
            index[sl] = where
            dist[sl] = np.sqrt(best)
        return index.reshape(x.shape), dist.reshape(x.shape)


def terrain(road, hills, x, z):
    """The hills with the road bed carved/filled in along the road (arrays)."""
    idx, dist = road.nearest(x, z)
    base = hills(x, z)
    w = 1.0 - smoothstep(ROAD_WIDTH / 2 + 0.5, ROAD_WIDTH / 2 + 0.5 + ROAD_BLEND, dist)
    return base * (1 - w) + road.h[idx] * w


class Grid:
    """The terrain heights on the regular grid, and a bilinear lookup."""

    def __init__(self, road, hills):
        self.nx = int(round((X1 - X0) / CELL))
        self.nz = int(round((Z1 - Z0) / CELL))
        self.xs = X0 + np.arange(self.nx + 1) * CELL
        self.zs = Z0 + np.arange(self.nz + 1) * CELL
        gx, gz = np.meshgrid(self.xs, self.zs)            # [row = z, col = x]
        self.h = terrain(road, hills, gx, gz)

    def height(self, x, z):
        fx = np.clip((np.asarray(x) - X0) / CELL, 0, self.nx - 1e-6)
        fz = np.clip((np.asarray(z) - Z0) / CELL, 0, self.nz - 1e-6)
        ix, iz = fx.astype(int), fz.astype(int)
        tx, tz = fx - ix, fz - iz
        h = self.h
        return ((h[iz, ix] * (1 - tx) + h[iz, ix + 1] * tx) * (1 - tz) +
                (h[iz + 1, ix] * (1 - tx) + h[iz + 1, ix + 1] * tx) * tz)


def write_floor(grid):
    h = grid.h
    dhdz, dhdx = np.gradient(h, CELL)
    n = np.stack([-dhdx, np.ones_like(h), -dhdz], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    rows, cols = h.shape
    with open(os.path.join(OUT, "forest_floor.obj"), "w") as fh:
        fh.write("# generated by generate_forest.py\nmtllib forest.mtl\no forest_floor\n")
        for j in range(rows):
            z = grid.zs[j]
            fh.write("".join("v %.2f %.3f %.2f\n" % (grid.xs[i], h[j, i], z)
                             for i in range(cols)))
        for j in range(rows):
            z = grid.zs[j]
            fh.write("".join("vt %.3f %.3f\n" % (grid.xs[i] / FLOOR_TILE, z / FLOOR_TILE)
                             for i in range(cols)))
        for j in range(rows):
            fh.write("".join("vn %.3f %.3f %.3f\n" % tuple(n[j, i]) for i in range(cols)))
        fh.write("usemtl floor\ns 1\n")
        for j in range(rows - 1):
            out = []
            for i in range(cols - 1):
                a = j * cols + i + 1
                b, c, d = a + 1, a + cols, a + cols + 1
                # counter-clockwise seen from above (+y)
                out.append("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, c, c, c, b, b, b))
                out.append("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (b, b, b, c, c, c, d, d, d))
            fh.write("".join(out))


def write_material_map(road):
    """A greyscale image: each pixel a FloorMaterial number. It covers the
    floor exactly (columns along +x, rows along +z, the first row the lowest
    z). Asphalt wherever the road ribbon is."""
    w = int(round((X1 - X0) / MATERIAL_STEP))
    hgt = int(round((Z1 - Z0) / MATERIAL_STEP))
    cx = X0 + (np.arange(w) + 0.5) * (X1 - X0) / w
    cz = Z0 + (np.arange(hgt) + 0.5) * (Z1 - Z0) / hgt
    gx, gz = np.meshgrid(cx, cz)
    _, dist = road.nearest(gx, gz)
    inside = (gz >= ROAD_START_Z) & (gz <= ROAD_END_Z)
    pixels = np.where((dist <= ROAD_WIDTH / 2) & inside, MATERIAL_ASPHALT, MATERIAL_GRASS)
    Image.fromarray(pixels.astype(np.uint8), "L").save(
        os.path.join(OUT, "forest_floor_materials.png"))
    return float((pixels == MATERIAL_ASPHALT).mean())


def write_road(road):
    """A ribbon laid on the road bed, from the start to the end of the road."""
    across = np.linspace(-1, 1, 5)
    step = 2
    ids = list(range(0, road.n, step))
    if ids[-1] != road.n - 1:
        ids.append(road.n - 1)
    verts, uvs, faces = [], [], []
    for row, k in enumerate(ids):
        nx, nz = -road.tz[k], road.tx[k]              # to the left of the travel
        v = k * PATH_STEP / ROAD_TILE
        for a in across:
            verts.append((road.x[k] + a * ROAD_WIDTH / 2 * nx, road.h[k] + ROAD_LIFT,
                          road.z[k] + a * ROAD_WIDTH / 2 * nz))
            uvs.append(((a + 1) / 2, v))
    q = len(across)
    for r in range(len(ids) - 1):
        for c in range(q - 1):
            a, b = r * q + c, r * q + c + 1
            cc, d = (r + 1) * q + c, (r + 1) * q + c + 1
            faces.append((a, b, cc))
            faces.append((b, d, cc))
    # face them up
    v0 = np.array(verts[faces[0][0]])
    nrm = np.cross(np.array(verts[faces[0][1]]) - v0, np.array(verts[faces[0][2]]) - v0)
    if nrm[1] < 0:
        faces = [(a, cc, b) for a, b, cc in faces]
    with open(os.path.join(OUT, "road.obj"), "w") as fh:
        fh.write("# generated by generate_forest.py\nmtllib forest.mtl\no road\n")
        fh.write("".join("v %.3f %.3f %.3f\n" % p for p in verts))
        fh.write("".join("vt %.4f %.4f\n" % t for t in uvs))
        fh.write("vn 0 1 0\nusemtl road\ns 1\n")
        fh.write("".join("f %d/%d/1 %d/%d/1 %d/%d/1\n" %
                         (a + 1, a + 1, b + 1, b + 1, cc + 1, cc + 1) for a, b, cc in faces))


def write_path(road):
    with open(os.path.join(OUT, "forest_path.txt"), "w") as fh:
        fh.write("# x z height tx tz, one point every %g m (generate_forest.py)\n" % PATH_STEP)
        for k in range(road.n):
            fh.write("%.3f %.3f %.3f %.5f %.5f\n" %
                     (road.x[k], road.z[k], road.h[k], road.tx[k], road.tz[k]))


def write_scene(road, heroes, start_index=14):
    """assets/scenes/forest.scene: what the web viewer shows of the forest map (the game
    builds it in code, src/world/ForestStage.cpp, from the same files). The RV and the
    signs stand where ForestStage puts them (START_Z_INDEX there)."""
    k = start_index
    x, z, h = road.x[k], road.z[k], road.h[k]
    lines = [
        "# The forest road, by day.",
        "#",
        "# What the web viewer (tools/scene_viewer/) shows of the map \"Bosque\". The game builds that",
        "# map in code (ForestStage, src/world/ForestStage.cpp) from the same files, so this one is",
        "# written by assets/forest/generate_forest.py: do not edit it, regenerate it.",
        "",
        "time_of_day   12",
        "day_duration  360",
        "procedural_sky forest",
        "sky     sky/skydome_plain.obj",
        "moon    -0.32 0.8 -0.5",
        "light   0.85 0.83 0.78",
        "fog     0.45 0.68 0.92",
        "",
        "player  ping/PenguinoAnimado.fbx  %.2f ground %.2f" % (x + 3.0, z - 0.5),
        "camera  0 1.6",
        "",
        "floor   forest/forest_floor.obj  0 0 0",
        "object  forest/road.obj          0 0 0   0 1",
        "",
        "object  rv/rv.obj  %.2f ground %.2f  0 1" % (x, z),
    ]
    for name, dx, dz in (("wheel_negx", -1.2, 2.4), ("wheel_posx", 1.2, 2.4),
                         ("wheel_negx", -1.2, -2.3), ("wheel_posx", 1.2, -2.3)):
        lines.append("object  rv/%s.obj  %.2f %.2f %.2f  0 1" % (name, x + dx, h + 0.5, z + dz))
    lines.append("")
    lines.append("# The trees (hero_trees.txt) of the first %d m of the road only, and with the" % SCENE_ROAD)
    lines.append("# simpler models (the game swaps three levels of detail with the distance). The")
    lines.append("# ones near the road sway in the wind.")
    for name, x_, y_, z_, yaw, scale, animated, solid, dist in heroes:
        if z_ > ROAD_START_Z + SCENE_ROAD + 60:
            continue
        model = name + ("_lod1" if dist < NEAR else "_lod2")
        lines.append("object  forest/%s.obj  %.2f %.2f %.2f  %.2f %.2f %s" %
                     (model, x_, y_, z_, yaw, scale, "sway" if animated else "lit"))
    lines.append("")
    for index, side in ((4, 1.0), (road.n - 12, -1.0)):
        sx = road.x[index] + side * (-road.tz[index]) * 5.2
        sz = road.z[index] + side * road.tx[index] * 5.2
        yaw = np.arctan2(road.x[index] - sx, road.z[index] - sz)
        lines.append("object  sign/sign.obj  %.2f ground %.2f  %.4f 1" % (sx, sz, yaw))
    scenes = os.path.join(OUT, "..", "scenes")
    with open(os.path.join(scenes, "forest.scene"), "w") as fh:
        fh.write("\n".join(lines) + "\n")


# --------------------------------------------------------------------- trees
def scatter(rng, road, grid, cell, dmin, dmax, density=None):
    """Points on a jittered grid, those between dmin and dmax from the road."""
    margin = 3.0
    xs = np.arange(X0 + margin, X1 - margin, cell)
    zs = np.arange(Z0 + margin, Z1 - margin, cell)
    gx, gz = np.meshgrid(xs, zs)
    gx = gx + rng.uniform(-0.42, 0.42, gx.shape) * cell
    gz = gz + rng.uniform(-0.42, 0.42, gz.shape) * cell
    gx, gz = gx.ravel(), gz.ravel()
    ok = (gx > X0 + margin) & (gx < X1 - margin) & (gz > Z0 + margin) & (gz < Z1 - margin)
    gx, gz = gx[ok], gz[ok]
    _, dist = road.nearest(gx, gz)
    keep = (dist >= dmin) & (dist < dmax)
    if density is not None:
        keep &= rng.random(len(gx)) < density(gx, gz)
    gx, gz, dist = gx[keep], gz[keep], dist[keep]
    return gx, gz, dist


def write_forest(road, grid, rng):
    """Every tree of the forest, in hero_trees.txt; returns the rows (model name, x, y, z, yaw,
    scale, animated, solid)."""
    patch = periodic_noise(128, 1.6, rng)

    def thin(x, z):   # a few thinner patches, never bare
        u = ((x - X0) / 37.0).astype(int) % 128
        v = ((z - Z0) / 37.0).astype(int) % 128
        return 0.75 + 0.25 * patch[v, u] / patch.max()

    names = list(generate_trees.MODEL_INFO)
    # a mixed forest: spruces most, then oaks, pines and birches
    weights = np.array([0.08, 0.07, 0.07, 0.08, 0.08, 0.14, 0.13, 0.13, 0.11, 0.11])[:len(names)]
    weights /= weights.sum()
    rows = []
    for cell, dmin, dmax, scale in ((NEAR_CELL, CLEAR, NEAR, (0.8, 1.15)),
                                    (FAR_CELL, NEAR, 1e9, (0.95, 1.35))):
        gx, gz, dist = scatter(rng, road, grid, cell, dmin, dmax, thin)
        model = rng.choice(len(names), size=len(gx), p=weights)
        scl = rng.uniform(scale[0], scale[1], len(gx))
        yaw = rng.uniform(0, 2 * np.pi, len(gx))
        y = grid.height(gx, gz) - 0.1 * scl
        for i in range(len(gx)):
            rows.append((names[model[i]], gx[i], float(y[i]), gz[i], yaw[i], scl[i],
                         int(dist[i] < ANIMATED), int(dist[i] < COLLIDE), float(dist[i])))
    with open(os.path.join(OUT, "hero_trees.txt"), "w") as fh:
        fh.write("# model x y z yaw scale animated solid deep trunk-radius height crown-radius "
                 "(generate_forest.py); deep: the deep forest, always extremely low poly\n")
        for name, x, y, z, yaw, scale, animated, solid, dist in rows:
            height, crown, trunk = generate_trees.MODEL_INFO[name]
            fh.write("%s %.2f %.2f %.2f %.2f %.2f %d %d %d %.3f %.2f %.2f\n" %
                     (name, x, y, z, yaw, scale, animated, solid, int(dist >= NEAR), trunk,
                      height, crown))
    return rows


if __name__ == "__main__":
    rng = np.random.default_rng(11)
    make_floor_texture(rng)
    make_road_texture(rng)
    write_mtl()
    hills = Hills(np.random.default_rng(5))
    road = Road(hills)
    print("road: %.0f m long, %.0f m wide, tightest bend radius %.0f m, bed height %.1f..%.1f m" %
          (road.length, ROAD_WIDTH, road.min_radius, road.h.min(), road.h.max()))
    assert road.min_radius > 60, "the road bends too tightly"
    grade = np.degrees(np.arctan(np.abs(np.diff(road.h)) / PATH_STEP)).max()
    print("steepest road grade: %.1f degrees" % grade)
    grid = Grid(road, hills)
    slope = np.hypot(*np.gradient(grid.h, CELL))
    print("terrain: %dx%d vertices, steepest slope %.0f degrees" %
          (grid.h.shape[1], grid.h.shape[0], np.degrees(np.arctan(slope.max()))))
    write_floor(grid)
    write_road(road)
    write_path(road)
    share = write_material_map(road)
    print("material map: %.2f%% of the floor is asphalt" % (100 * share))
    generate_trees.main()
    rows = write_forest(road, grid, np.random.default_rng(13))
    write_scene(road, rows)
    print("%d trees: %d sway in the wind, %d have a solid trunk" %
          (len(rows), sum(r[6] for r in rows), sum(r[7] for r in rows)))
    print("done")
