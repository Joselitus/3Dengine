#!/usr/bin/env python3
"""Procedurally generates the detailed ("hero") trees of the forest map.

Usage: python3 generate_trees.py        (writes next to this script; generate_forest.py
calls it and places the trees along the road)
Needs numpy and Pillow. Everything is seeded, so the output is reproducible.

Unlike the low-poly forest (tree_*.obj, thousands of them), each of these is a
tree of its own: a trunk that tapers and forks into branches (tubes swept along
curves, level after level, growing towards the light) covered with cards of
leaves or needles, which are quads with a painted RGBA texture (the game discards
the transparent texels). Four species, a few variants of each:

  oak     thick trunk and a wide crown of lobed leaves
  birch   slim white trunk with dark marks, light small leaves on drooping twigs
  spruce  straight trunk and whorls of drooping branches covered with needles
  pine    tall bare trunk with a flat crown of tufts of long needles

Each model stands on y = 0 with its trunk on the y axis. The cards have their
normals pointing away from the middle of the crown, so a crown is shaded like a
soft volume, not like a pile of flat cards. The leaves and the tops of the trees
sway in the wind in the game (the vertex shader moves them by their height and
their distance from the trunk).
"""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.dirname(os.path.abspath(__file__))
TAU = 2 * np.pi


# ------------------------------------------------------------------ textures
def periodic_noise(n, power, rng, stretch=(1.0, 1.0)):
    """Tileable noise: white noise filtered with 1/f^power in Fourier space. `stretch` > 1
    in one axis makes the features longer along it (streaks)."""
    f = np.fft.fft2(rng.standard_normal((n, n)))
    fy = np.fft.fftfreq(n)[:, None] * stretch[0]
    fx = np.fft.fftfreq(n)[None, :] * stretch[1]
    r = np.sqrt(fx * fx + fy * fy)
    r[0, 0] = 1.0
    f = f / r ** power
    f[0, 0] = 0
    a = np.real(np.fft.ifft2(f))
    return (a - a.min()) / (a.max() - a.min())


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def mix(c0, c1, t):
    t = np.clip(t, 0, 1)[..., None]
    return np.array(c0, dtype=float) * (1 - t) + np.array(c1, dtype=float) * t


def save_jpg(arr, name):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(
        os.path.join(OUT, name), quality=93)


def make_barks(rng, n=512):
    """Four bark textures; u runs round the trunk and v along it (both tile)."""
    # oak: deep vertical furrows, dark brown and grey
    streak = periodic_noise(n, 1.3, rng, stretch=(0.18, 1.0))
    fine = periodic_noise(n, 0.5, rng)
    u = np.arange(n)[None, :] / n
    ridge = 0.5 + 0.5 * np.sin(TAU * (9 * u + 1.2 * (streak - 0.5)))
    t = 0.55 * ridge * (0.5 + streak) / 1.5 + 0.30 * streak + 0.15 * fine
    col = mix((62, 48, 38), (158, 136, 114), t ** 1.1)
    save_jpg(col, "bark_oak.jpg")

    # birch: white, with dark horizontal marks and a few grey patches
    big = periodic_noise(n, 1.2, rng)
    marks = smooth(0.62, 0.74, periodic_noise(n, 0.9, rng, stretch=(1.0, 0.12)))
    col = mix((232, 230, 222), (190, 186, 176), big * 0.8)
    col = col * (1 - 0.9 * marks[..., None]) + np.array([34, 30, 28]) * 0.9 * marks[..., None]
    col += (fine[..., None] - 0.5) * 18
    save_jpg(col, "bark_birch.jpg")

    # spruce: reddish-brown scaly plates
    cells = periodic_noise(n, 1.0, rng, stretch=(0.5, 1.0))
    edges = smooth(0.46, 0.52, cells) * (1 - smooth(0.52, 0.58, cells))
    col = mix((98, 62, 46), (166, 112, 82), 0.6 * periodic_noise(n, 0.8, rng) + 0.4 * cells)
    col = col * (1 - 0.55 * edges[..., None])
    save_jpg(col, "bark_spruce.jpg")

    # pine: orange-red plates, grey and rough lower down
    plates = periodic_noise(n, 1.1, rng, stretch=(0.3, 1.0))
    edges = smooth(0.40, 0.48, plates) * (1 - smooth(0.48, 0.55, plates))
    col = mix((120, 68, 44), (214, 130, 80), 0.5 * periodic_noise(n, 0.7, rng) + 0.5 * plates)
    col = col * (1 - 0.6 * edges[..., None])
    save_jpg(col, "bark_pine.jpg")


def leaf_polygon(kind, rng):
    """Outline of one leaf, along +x from 0 to 1, as (x, y) points."""
    t = np.linspace(0, 1, 40)
    if kind == "oak":                       # lobed
        half = 0.30 * np.sin(np.pi * t ** 0.8) * (1 + 0.28 * np.sin(TAU * 3.5 * t))
    elif kind == "birch":                   # ovate, pointed, a little toothed
        half = 0.36 * np.sin(np.pi * t ** 0.75) ** 0.9 * (1 + 0.08 * np.sin(TAU * 9 * t))
    else:
        half = 0.3 * np.sin(np.pi * t)
    return np.concatenate([np.stack([t, half], 1), np.stack([t[::-1], -half[::-1]], 1)])


def paint_leaf(draw, kind, x, y, length, angle, rng, dark, light):
    """One leaf, with a lighter midrib and veins, into an RGBA ImageDraw."""
    pts = leaf_polygon(kind, rng) * length
    c, s = np.cos(angle), np.sin(angle)
    rot = np.stack([x + c * pts[:, 0] - s * pts[:, 1], y + s * pts[:, 0] + c * pts[:, 1]], 1)
    shade = rng.uniform(0, 1)
    colour = tuple(int(dark[i] + (light[i] - dark[i]) * shade) for i in range(3)) + (255,)
    draw.polygon([tuple(p) for p in rot], fill=colour)
    tip = (x + c * length * 0.95, y + s * length * 0.95)
    rib = tuple(min(255, int(v * 1.25 + 18)) for v in colour[:3]) + (255,)
    draw.line([(x, y), tip], fill=rib, width=max(1, int(length / 28)))
    for k in (0.3, 0.5, 0.7):               # veins
        bx, by = x + c * length * k, y + s * length * k
        for side in (-1, 1):
            a2 = angle + side * 0.9
            draw.line([(bx, by), (bx + np.cos(a2) * length * 0.2, by + np.sin(a2) * length * 0.2)],
                      fill=rib, width=max(1, int(length / 50)))


def finish_tile(img):
    """Downsizes a supersampled tile, and fills the transparent texels with the colour of
    their neighbours (so that mipmaps do not get dark or pale fringes)."""
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    a = np.array(img).astype(float)
    alpha = a[..., 3:4] / 255.0
    rgb = a[..., :3]
    mean = (rgb * alpha).sum((0, 1)) / max(alpha.sum(), 1e-6)
    blurred = np.array(Image.fromarray(np.uint8(rgb * alpha + mean * (1 - alpha))).filter(
        ImageFilter.GaussianBlur(6))).astype(float)
    filled = np.where(alpha > 0.5, rgb, blurred * (1 - alpha) + rgb * alpha)
    return np.concatenate([np.clip(filled, 0, 255), a[..., 3:4]], axis=2).astype(np.uint8)


def make_leaf_atlas(name, kind, rng, tints, size=256, ss=3):
    """A 2 x 2 atlas of cards (four variants), `size` pixels each."""
    atlas = np.zeros((2 * size, 2 * size, 4), np.uint8)
    for variant in range(4):
        dark, light = tints[variant % len(tints)]
        big = size * ss
        img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        if kind in ("oak", "birch"):
            # a twig along the card with leaves on both sides, all pointing outwards
            stem_y = big * 0.5
            draw.line([(big * 0.04, stem_y), (big * 0.96, stem_y + rng.uniform(-30, 30))],
                      fill=(70, 52, 34, 255), width=int(big / 70))
            count = 9 if kind == "oak" else 15
            length = big * (0.38 if kind == "oak" else 0.26)
            for k in range(count):
                x = big * (0.06 + 0.86 * k / max(count - 1, 1))
                side = -1 if k % 2 else 1
                angle = side * rng.uniform(0.6, 1.1) + rng.uniform(-0.15, 0.15)
                paint_leaf(draw, kind, x, stem_y, length * rng.uniform(0.8, 1.15), angle,
                           rng, dark, light)
            for k in range(3):              # a few more filling the gaps
                paint_leaf(draw, kind, big * rng.uniform(0.2, 0.8), stem_y + rng.uniform(-60, 60),
                           length * 0.8, rng.uniform(0, TAU), rng, dark, light)
        elif kind == "spruce":
            # a feathery sprig: needles all along a stem, longer in the middle
            stem = (big * 0.5, big * 0.04), (big * 0.5, big * 0.96)
            draw.line(stem, fill=(80, 56, 40, 255), width=int(big / 60))
            for k in range(150):
                y = big * (0.05 + 0.9 * k / 149)
                taper = np.sin(np.pi * (0.1 + 0.8 * k / 149)) ** 0.6
                for side in (-1, 1):
                    L = big * 0.46 * taper * rng.uniform(0.7, 1.0)
                    a = np.arctan2(1, side * 1.0) + rng.uniform(-0.25, 0.25)
                    ex, ey = big * 0.5 + side * L * np.cos(0.55), y + L * 0.35
                    shade = rng.uniform(0, 1)
                    colour = tuple(int(dark[i] + (light[i] - dark[i]) * shade) for i in range(3)) + (255,)
                    draw.line([(big * 0.5, y), (ex, ey)], fill=colour, width=max(2, int(big / 90)))
        else:                                # pine: a tuft of long needles
            cx, cy = big * 0.5, big * 0.5
            for k in range(170):
                a = rng.uniform(0, TAU)
                L = big * rng.uniform(0.2, 0.47)
                bend = rng.uniform(-0.3, 0.3)
                mid = (cx + np.cos(a + bend) * L * 0.55, cy + np.sin(a + bend) * L * 0.55)
                end = (cx + np.cos(a + 2 * bend) * L, cy + np.sin(a + 2 * bend) * L)
                shade = rng.uniform(0, 1)
                colour = tuple(int(dark[i] + (light[i] - dark[i]) * shade) for i in range(3)) + (255,)
                draw.line([(cx, cy), mid, end], fill=colour, width=max(2, int(big / 120)))
        tile = finish_tile(img.resize((size, size), Image.LANCZOS))
        row, col = divmod(variant, 2)
        atlas[row * size:(row + 1) * size, col * size:(col + 1) * size] = tile
    Image.fromarray(atlas, "RGBA").save(os.path.join(OUT, name))


def make_leaves(rng):
    make_leaf_atlas("leaves_oak.png", "oak", rng,
                    [((30, 70, 24), (86, 138, 40)), ((44, 84, 24), (112, 150, 44)),
                     ((62, 90, 24), (140, 150, 52)), ((26, 64, 30), (72, 124, 44))])
    make_leaf_atlas("leaves_birch.png", "birch", rng,
                    [((74, 120, 36), (150, 184, 70)), ((90, 130, 40), (176, 196, 78)),
                     ((60, 110, 40), (128, 172, 66)), ((120, 130, 40), (200, 190, 80))])
    make_leaf_atlas("leaves_spruce.png", "spruce", rng,
                    [((14, 44, 30), (36, 88, 58)), ((20, 54, 34), (52, 104, 62)),
                     ((12, 40, 38), (30, 80, 70)), ((26, 56, 28), (62, 108, 54))])
    make_leaf_atlas("leaves_pine.png", "pine", rng,
                    [((30, 70, 34), (92, 140, 60)), ((40, 80, 30), (110, 150, 56)),
                     ((24, 62, 38), (74, 124, 66)), ((50, 84, 30), (124, 156, 58))])


# ------------------------------------------------------------------ geometry
def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


def perpendicular(d, rng):
    """A random unit vector perpendicular to d."""
    r = rng.standard_normal(3)
    r -= d * np.dot(r, d)
    return unit(r)


def rotate_towards(d, axis, angle):
    """d turned by `angle` around the unit vector `axis`."""
    c, s = np.cos(angle), np.sin(angle)
    return d * c + np.cross(axis, d) * s + axis * np.dot(axis, d) * (1 - c)


class TreeModel:
    """Vertices, normals, uvs and faces of a tree, by material."""

    def __init__(self, bark_material, leaf_material, rng, lod, seed):
        self.rng = rng
        # The level of detail: the branches grow from `rng` whatever the level (so every level
        # is the same tree), while the leaves and the frames of the tubes have their own
        self.lod = lod
        self.leaf_rng = np.random.default_rng(seed + 9999)
        self.aux = np.random.default_rng(seed + 4242)
        self.parts = {bark_material: ([], [], [], []), leaf_material: ([], [], [], [])}
        self.bark, self.leaf = bark_material, leaf_material
        self.cards = []       # (centre, 4 corners, tile) kept to set their normals at the end
        self.trunk_radius = 0.3

    def add(self, material, v, n, uv, faces):
        pv, pn, puv, pf = self.parts[material]
        base = len(pv)
        pv.extend(v)
        pn.extend(n)
        puv.extend(uv)
        pf.extend((base + a, base + b, base + c) for a, b, c in faces)

    def tube(self, points, radii, sides=7, v_scale=0.5, tip=False):
        """A tube along `points` (N x 3) with a radius at each one, parallel-transported;
        `tip` closes the far end to a point."""
        points = np.asarray(points, float)
        N = len(points)
        tang = np.gradient(points, axis=0)
        tang /= np.maximum(np.linalg.norm(tang, axis=1, keepdims=True), 1e-9)
        sides = max(4, int(round(sides * self.lod["sides"])))
        side = perpendicular(tang[0], self.aux)
        lengths = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(points, axis=0), axis=1))])
        v, n, uv, faces = [], [], [], []
        u_reps = max(1.0, round(radii[0] * 7.0))   # the bark repeats round thick trunks
        for k in range(N):
            t = tang[k]
            side = unit(side - t * np.dot(side, t))
            other = np.cross(t, side)
            for s in range(sides + 1):
                th = TAU * s / sides
                radial = np.cos(th) * side + np.sin(th) * other
                v.append(points[k] + radii[k] * radial)
                n.append(radial)
                uv.append((s / sides * u_reps, lengths[k] * v_scale))
        for k in range(N - 1):
            for s in range(sides):
                a = k * (sides + 1) + s
                b, c, d = a + 1, a + sides + 1, a + sides + 2
                faces.append((a, c, b))
                faces.append((b, c, d))
        if tip:
            apex = len(v)
            v.append(points[-1] + tang[-1] * radii[-1] * 1.5)
            n.append(tang[-1])
            uv.append((0.5, (lengths[-1] + radii[-1]) * v_scale))
            last = (N - 1) * (sides + 1)
            for s in range(sides):
                faces.append((last + s, last + s + 1, apex))
        self.add(self.bark, v, n, uv, faces)

    def card(self, centre, right, up, width, height, tile):
        """A leaf card: a quad of `width` x `height` centred at `centre`, spanned by the unit
        vectors `right` and `up`, showing tile 0..3 of the 2 x 2 atlas."""
        corners = [centre + right * width * sx * 0.5 + up * height * sy * 0.5
                   for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        self.cards.append((centre, corners, tile))

    def finish(self, crown_centre, crown_up=0.35):
        """Writes the cards, with normals pointing away from the middle of the crown (and a
        bit up)."""
        for centre, corners, tile in self.cards:
            out = unit(centre - crown_centre)
            normal = unit(out + np.array([0.0, crown_up, 0.0]))
            row, col = divmod(tile, 2)
            # (the card's own v runs up the texture: v = 1 - row)
            uvs = [(col * 0.5 + 0.004, 1 - (row * 0.5 + 0.5) + 0.004),
                   (col * 0.5 + 0.5 - 0.004, 1 - (row * 0.5 + 0.5) + 0.004),
                   (col * 0.5 + 0.5 - 0.004, 1 - row * 0.5 - 0.004),
                   (col * 0.5 + 0.004, 1 - row * 0.5 - 0.004)]
            self.add(self.leaf, corners, [normal] * 4, uvs, [(0, 1, 2), (0, 2, 3)])

    def write(self, name, mtl="hero.mtl"):
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write("# generated by generate_trees.py\nmtllib %s\no %s\n" %
                     (mtl, os.path.splitext(name)[0]))
            offset = 0
            tris = 0
            for material, (v, n, uv, f) in self.parts.items():
                if not f:
                    continue
                fh.write("usemtl %s\ns 1\n" % material)
                fh.write("".join("v %.3f %.3f %.3f\n" % tuple(p) for p in v))
                fh.write("".join("vt %.4f %.4f\n" % tuple(t) for t in uv))
                fh.write("".join("vn %.3f %.3f %.3f\n" % tuple(q) for q in n))
                fh.write("".join("f %d/%d/%d %d/%d/%d %d/%d/%d\n" %
                                 tuple(offset + i + 1 for i in (a, a, a, b, b, b, c, c, c))
                                 for a, b, c in f))
                offset += len(v)
                tris += len(f)
        return tris


def bounds(model):
    pts = np.array([p for part in model.parts.values() for p in part[0]])
    return pts.min(axis=0), pts.max(axis=0)


# ------------------------------------------------------------------- species
def grow_branch(model, start, direction, length, radius, level, spec, rng, leafy=None):
    """A branch from `start` towards `direction`: it bends a little as it grows (and
    towards the light), forks into children and carries leaf cards at its thin end."""
    segs = max(4, int(length / (spec["seg"] * model.lod["seg"])))
    d = unit(direction)
    p = np.array(start, float)
    pts, radii = [p.copy()], [radius]
    for i in range(1, segs + 1):
        d = unit(d + rng.normal(0, spec["wobble"], 3) + np.array([0, spec["tropism"], 0]))
        p = p + d * (length / segs)
        pts.append(p.copy())
        radii.append(radius * (1 - 0.82 * i / segs) + 0.004)
    if level <= model.lod["max_level"]:
        model.tube(pts, radii, sides=spec["sides"][min(level, len(spec["sides"]) - 1)],
                   v_scale=spec["v_scale"], tip=True)
    pts = np.array(pts)

    if level < spec["levels"]:
        count = int(length * spec["fork"][level])
        for k in range(count):
            t = rng.uniform(spec["fork_from"], 0.97)
            idx = min(int(t * segs), segs - 1)
            base = pts[idx]
            axis = unit(pts[idx + 1] - pts[idx])
            ang = rng.uniform(*spec["fork_angle"])
            child_dir = rotate_towards(axis, perpendicular(axis, rng), ang)
            child_len = length * rng.uniform(*spec["child_len"]) * (1 - 0.4 * t)
            child_r = radii[idx] * spec["child_radius"]
            if child_len > 0.35:
                grow_branch(model, base, child_dir, child_len, child_r, level + 1, spec, rng)
    # leaves on the thin, young wood
    if level >= spec["leaf_level"]:
        lrng = model.leaf_rng
        n = int(spec["leaf_cards"](length, level) * model.lod["cards"] + lrng.random())
        for k in range(n):
            t = lrng.uniform(0.25, 1.0) ** 0.7
            centre = np.array(pts[min(int(t * segs), segs)]) + lrng.normal(0, spec["leaf_spread"], 3)
            axis = unit(lrng.standard_normal(3))
            right = perpendicular(axis, lrng)
            up = np.cross(axis, right)
            size = spec["leaf_size"] * lrng.uniform(0.75, 1.25) * model.lod["size"]
            model.card(centre, right, up, size, size, int(lrng.integers(0, 4)))


OAK = dict(seg=0.8, wobble=0.10, tropism=0.015, sides=[10, 8, 6, 5], levels=3, v_scale=0.28,
           fork=[1.1, 1.2, 1.2], fork_from=0.28, fork_angle=(0.55, 1.1),
           child_len=(0.55, 0.8), child_radius=0.55, leaf_level=2,
           leaf_cards=lambda length, level: int(3 + 3.0 * length), leaf_spread=0.30, leaf_size=0.9)
BIRCH = dict(seg=0.7, wobble=0.07, tropism=0.01, sides=[7, 6, 5, 4], levels=3, v_scale=0.3,
             fork=[1.3, 1.5, 1.4], fork_from=0.35, fork_angle=(0.35, 0.7),
             child_len=(0.45, 0.7), child_radius=0.5, leaf_level=2,
             leaf_cards=lambda length, level: int(2 + 1.5 * length), leaf_spread=0.22, leaf_size=0.62)


def make_broadleaf(spec, bark, leaf, seed, height, trunk_radius, name, lean=0.05, lod=None):
    rng = np.random.default_rng(seed)
    m = TreeModel(bark, leaf, rng, lod or LOD[0], seed)
    direction = unit(np.array([rng.normal(0, lean), 1.0, rng.normal(0, lean)]))
    # the trunk grows as the first branch, with a flared foot
    grow_branch(m, (0, -0.15, 0), direction, height * 0.8, trunk_radius, 0, spec, rng)
    m.trunk_radius = trunk_radius
    cards = np.array([c[0] for c in m.cards])
    centre = cards.mean(axis=0) - np.array([0, 0.3, 0])
    m.finish(centre)
    lo, hi = bounds(m)
    crown = float(np.linalg.norm(cards[:, [0, 2]], axis=1).max() + 0.5)
    tris = m.write(name)
    return tris, float(hi[1]), crown, trunk_radius


def make_spruce(seed, height, trunk_radius, name, droop=0.5, leaf="leaves_spruce", lod=None):
    rng = np.random.default_rng(seed)
    lod = lod or LOD[0]
    m = TreeModel("bark_spruce", leaf, rng, lod, seed)
    lrng = m.leaf_rng
    n = max(4, int(height / (0.6 * lod["seg"])))
    ys = np.linspace(-0.15, height, n)
    xs = np.cumsum(rng.normal(0, 0.012, n))
    zs = np.cumsum(rng.normal(0, 0.012, n))
    radii = trunk_radius * (1 - 0.93 * (np.maximum(ys, 0) / height) ** 0.9) + 0.01
    m.tube(np.stack([xs, ys, zs], 1), radii, sides=10, v_scale=0.3, tip=True)
    m.trunk_radius = trunk_radius
    y = 1.4
    crown_r = 0.0
    while y < height * 0.985:
        k = y / height
        reach = 3.3 * (1 - k) ** 0.95 + 0.25
        branches = 6 if k < 0.6 else 5
        phase = rng.uniform(0, TAU)
        for b in range(branches):
            az = phase + TAU * (b + rng.uniform(-0.2, 0.2)) / branches
            outward = np.array([np.cos(az), 0.0, np.sin(az)])
            L = reach * rng.uniform(0.8, 1.1)
            segs = max(4, int(L / 0.5))
            p = np.array([xs[min(int(k * n), n - 1)], y, zs[min(int(k * n), n - 1)]])
            pts, d = [p.copy()], unit(outward + np.array([0, 0.08, 0]))
            for i in range(segs):
                # out, drooping more with the weight, the tip curling back up
                sag = -droop * (i / segs) + 0.9 * max(0.0, i / segs - 0.75)
                d = unit(outward + np.array([0, sag * 0.7 + 0.05, 0]) + rng.normal(0, 0.04, 3))
                p = p + d * (L / segs)
                pts.append(p.copy())
            pts = np.array(pts)
            if lod["max_level"] >= 3:
                m.tube(pts, np.linspace(0.05 * (1 - 0.7 * k) + 0.012, 0.008, len(pts)), sides=5,
                       v_scale=0.5, tip=True)
            crown_r = max(crown_r, L)
            # sprigs of needles along the branch: cards laid along it and one crossing
            count = int(L / 0.42) + 2
            for i in range(count):
                if lrng.random() > lod["keep"]:   # fewer, bigger sprigs
                    continue
                t = (i + 0.5) / count
                idx = min(int(t * segs), segs - 1)
                centre = pts[idx] + np.array([0, 0.03, 0])
                along = unit(pts[idx + 1] - pts[idx])
                sprig = 1.15 * (1 - 0.45 * k) * lrng.uniform(0.9, 1.15) * lod["size"] ** 0.7
                flat = unit(np.cross(along, np.array([0, 1.0, 0])))
                tilt = lrng.uniform(-0.25, 0.25)
                up_v = unit(np.cross(flat, along) + flat * tilt)
                m.card(centre + flat * 0.1, along, flat, sprig, sprig * 0.75, int(lrng.integers(0, 4)))
                m.card(centre - flat * 0.1, along, flat, sprig, sprig * 0.75, int(lrng.integers(0, 4)))
                m.card(centre, along, up_v, sprig, sprig * 0.5, int(lrng.integers(0, 4)))
        y += rng.uniform(0.5, 0.62)
    cards = np.array([c[0] for c in m.cards])
    m.finish(np.array([0.0, height * 0.5, 0.0]), crown_up=0.15)
    tris = m.write(name)
    return tris, height, float(crown_r + 0.4), trunk_radius


def make_pine(seed, height, trunk_radius, name, lod=None):
    rng = np.random.default_rng(seed)
    lod = lod or LOD[0]
    m = TreeModel("bark_pine", "leaves_pine", rng, lod, seed)
    lrng = m.leaf_rng
    n = max(4, int(height / (0.7 * lod["seg"])))
    ys = np.linspace(-0.15, height, n)
    bend = np.cumsum(rng.normal(0, 0.02, (n, 2)), axis=0) + np.linspace(0, 1, n)[:, None] ** 2 * rng.normal(0, 0.8, 2)
    radii = trunk_radius * (1 - 0.8 * (np.maximum(ys, 0) / height) ** 1.3) + 0.02
    trunk = np.stack([bend[:, 0], ys, bend[:, 1]], 1)
    m.tube(trunk, radii, sides=10, v_scale=0.3, tip=True)
    m.trunk_radius = trunk_radius
    crown_r = 0.0
    start = height * 0.55
    branches = int(rng.integers(10, 14))
    for b in range(branches):
        t = start / height + (0.97 - start / height) * (b / branches) ** 0.8
        y = t * height
        idx = min(int(t * (n - 1)), n - 2)
        az = TAU * (b * 0.618 + rng.uniform(-0.05, 0.05))
        L = rng.uniform(1.8, 3.8) * (1 - 0.55 * (t - start / height) / (1 - start / height))
        outward = np.array([np.cos(az), 0.0, np.sin(az)])
        segs = max(4, int(L / 0.5))
        p = trunk[idx].copy()
        pts = [p.copy()]
        for i in range(segs):
            d = unit(outward + np.array([0, 0.5 + 0.2 * np.sin(i / segs * 2), 0]) + rng.normal(0, 0.12, 3))
            p = p + d * (L / segs)
            pts.append(p.copy())
        pts = np.array(pts)
        if lod["max_level"] >= 3:
            m.tube(pts, np.linspace(0.07 * (1 - 0.5 * t) + 0.02, 0.012, len(pts)), sides=5,
                   v_scale=0.5, tip=True)
        crown_r = max(crown_r, float(np.linalg.norm(pts[-1][[0, 2]] - trunk[idx][[0, 2]]) + 1.0))
        # tufts of long needles near the end of the branch: three crossed cards each
        for i in range(max(1, int(round(lrng.integers(4, 8) * max(lod["cards"], 0.3))))):
            f = lrng.uniform(0.55, 1.0)
            centre = pts[min(int(f * segs), segs)] + lrng.normal(0, 0.12, 3)
            size = lrng.uniform(1.1, 1.6) * lod["size"] ** 0.7
            tile = int(lrng.integers(0, 4))
            axis = unit(lrng.standard_normal(3))
            r1 = perpendicular(axis, lrng)
            u1 = np.cross(axis, r1)
            for turn in range(3):
                ang = turn * np.pi / 3
                a = rotate_towards(r1, axis, ang)
                b2 = rotate_towards(u1, axis, ang)
                m.card(centre, a, b2, size, size, tile)
    cards = np.array([c[0] for c in m.cards])
    m.finish(np.array([trunk[-1][0], height * 0.82, trunk[-1][2]]), crown_up=0.5)
    tris = m.write(name)
    return tris, height, float(crown_r + 0.4), trunk_radius


def write_mtl():
    mats = [("bark_oak", "bark_oak.jpg"), ("bark_birch", "bark_birch.jpg"),
            ("bark_spruce", "bark_spruce.jpg"), ("bark_pine", "bark_pine.jpg"),
            ("leaves_oak", "leaves_oak.png"), ("leaves_birch", "leaves_birch.png"),
            ("leaves_spruce", "leaves_spruce.png"), ("leaves_pine", "leaves_pine.png")]
    with open(os.path.join(OUT, "hero.mtl"), "w") as fh:
        fh.write("# generated by generate_trees.py\n")
        for name, tex in mats:
            fh.write("newmtl %s\nNs 5\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\nillum 1\nmap_Kd %s\n\n"
                     % (name, tex))


# Levels of detail: the fraction of the leaf cards that are kept, how much bigger they get to
# cover the same crown, the tubes' sides and the deepest level of branches that is drawn. The
# game swaps them with the distance (GameObject::addDetail)
# (`keep` is the same for the conifers, whose cards are sprigs along their branches; `seg` makes
# the segments of the branches longer: fewer rings of the tubes). The last level is extremely
# low poly: a trunk and a dozen big cards, 250 triangles or so; it is what the far trees and
# the whole deep forest use.
LOD = [dict(cards=1.0, size=1.0, sides=1.0, max_level=9, keep=1.0, seg=1.0),
       dict(cards=0.3, size=1.55, sides=0.7, max_level=2, keep=0.5, seg=1.0),
       dict(cards=0.03, size=4.2, sides=0.4, max_level=0, keep=0.02, seg=3.0)]
LOD_SUFFIX = ["", "_lod1", "_lod2"]

# model name -> (height m, trunk radius at the foot m, generator)
MODEL_INFO = {}   # name -> (height, crown radius, trunk radius): filled by main()


def main():
    rng = np.random.default_rng(21)
    make_barks(rng)
    make_leaves(rng)
    write_mtl()
    specs = [
        ("hero_oak_a", lambda n, l: make_broadleaf(OAK, "bark_oak", "leaves_oak", 101, 13.0, 0.55, n, lod=l)),
        ("hero_oak_b", lambda n, l: make_broadleaf(OAK, "bark_oak", "leaves_oak", 102, 11.0, 0.48, n, 0.09, lod=l)),
        ("hero_oak_c", lambda n, l: make_broadleaf(OAK, "bark_oak", "leaves_oak", 103, 15.0, 0.62, n, lod=l)),
        ("hero_birch_a", lambda n, l: make_broadleaf(BIRCH, "bark_birch", "leaves_birch", 111, 13.0, 0.2, n, 0.08, lod=l)),
        ("hero_birch_b", lambda n, l: make_broadleaf(BIRCH, "bark_birch", "leaves_birch", 112, 15.0, 0.22, n, 0.06, lod=l)),
        ("hero_spruce_a", lambda n, l: make_spruce(121, 16.0, 0.38, n, lod=l)),
        ("hero_spruce_b", lambda n, l: make_spruce(122, 13.0, 0.32, n, droop=0.7, lod=l)),
        ("hero_spruce_c", lambda n, l: make_spruce(123, 19.0, 0.42, n, droop=0.4, lod=l)),
        ("hero_pine_a", lambda n, l: make_pine(131, 17.0, 0.34, n, lod=l)),
        ("hero_pine_b", lambda n, l: make_pine(132, 14.0, 0.3, n, lod=l)),
    ]
    for name, build in specs:
        counts = []
        for level, lod in enumerate(LOD):
            tris, height, crown, trunk = build(name + LOD_SUFFIX[level] + ".obj", lod)
            counts.append(tris)
            if level == 0:
                MODEL_INFO[name] = (height, crown, trunk)
        print("%-14s %6d / %5d / %4d triangles, %4.1f m tall, crown radius %.1f m" %
              ((name,) + tuple(counts) + (MODEL_INFO[name][0], MODEL_INFO[name][1])))


if __name__ == "__main__":
    main()
