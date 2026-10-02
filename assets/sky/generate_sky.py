#!/usr/bin/env python3
"""Generates the night sky: night_sky.jpg (equirectangular, stars + Milky Way +
moon) and skydome.obj (inside-out sphere that uses it).

Usage: python3 generate_sky.py        (needs numpy and Pillow)

MOON_DIR must match `moonDir` in src/test.cpp.
"""
import os
import numpy as np
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))
W, H = 4096, 2048
DOME_RADIUS = 60.0
MOON_DIR = np.array([-0.32, 0.26, -0.91])
MOON_DIR /= np.linalg.norm(MOON_DIR)
STAR_BRIGHTNESS = 0.55  # scales stars and the Milky Way
MOON_RADIUS = 0.040     # radians (much bigger than the real moon, on purpose)

HORIZON = np.array([0.035, 0.055, 0.110])   # also the fog colour in test.cpp
ZENITH = np.array([0.003, 0.007, 0.028])
GROUND = np.array([0.012, 0.018, 0.038])

rng = np.random.default_rng(11)


def directions():
    """Unit direction for every pixel; row 0 is straight up."""
    u = (np.arange(W) + 0.5) / W
    v = (np.arange(H) + 0.5) / H
    phi = u * 2 * np.pi
    el = (0.5 - v) * np.pi
    ce = np.cos(el)[:, None]
    return np.stack([ce * np.cos(phi)[None, :],
                     np.sin(el)[:, None] * np.ones((1, W)),
                     ce * np.sin(phi)[None, :]], axis=-1), el


def noise_map(power):
    f = np.fft.fft2(rng.standard_normal((H, W)))
    fy = np.fft.fftfreq(H)[:, None]
    fx = np.fft.fftfreq(W)[None, :]
    r = np.sqrt(fx * fx + fy * fy)
    r[0, 0] = 1
    a = np.real(np.fft.ifft2(f / r ** power))
    return (a - a.min()) / (a.max() - a.min())


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def main():
    d, el = directions()

    # --- background gradient
    h = np.clip(el / (np.pi / 2), -1, 1)[:, None, None]
    sky = ZENITH + (HORIZON - ZENITH) * (1 - np.clip(h, 0, 1)) ** 2.2
    below = np.clip(-h, 0, 1)
    img = np.where(h >= 0, sky, HORIZON + (GROUND - HORIZON) * np.sqrt(below))
    img = np.broadcast_to(img, (H, W, 3)).copy()

    # --- Milky Way: a tilted band modulated by noise, with dark dust lanes
    n = np.array([0.35, 0.80, 0.50])
    n /= np.linalg.norm(n)
    b = d @ n
    band = np.exp(-(b / 0.20) ** 2)
    cloud = noise_map(1.6)
    fine = noise_map(0.9)
    dust = smoothstep(0.45, 0.65, noise_map(2.0))
    mw = band * (0.25 + 0.9 * cloud) * (1 - 0.55 * dust)
    mw *= smoothstep(-0.05, 0.25, d[..., 1])        # fade near the horizon
    mw_col = np.array([0.55, 0.60, 0.85]) * mw[..., None] * 0.30
    mw_col += np.array([0.80, 0.55, 0.40]) * (band * cloud * fine)[..., None] * 0.05
    img += mw_col * STAR_BRIGHTNESS

    # --- stars
    def draw_star(x, y, color, size):
        """Round gaussian dot. x is stretched by 1/cos(elevation)."""
        s = 0.6 + size
        stretch = 1.0 / max(np.cos(el[int(y)]), 0.15)
        rx = int(np.ceil(3.2 * s * stretch)) + 1
        ry = int(np.ceil(3.2 * s)) + 1
        ys = np.arange(int(y) - ry, int(y) + ry + 1)
        xs = np.arange(int(x) - rx, int(x) + rx + 1)
        ok_y = (ys >= 0) & (ys < H)
        gy, gx = np.meshgrid(ys, xs, indexing="ij")
        g = np.exp(-(((gx + 0.5 - x) / stretch) ** 2 + (gy + 0.5 - y) ** 2) / (2 * s * s))
        g = g[ok_y]
        gx = gx[ok_y] % W
        gy = gy[ok_y]
        img[gy, gx] += g[..., None] * color * STAR_BRIGHTNESS

    n_stars = 14000
    # Denser along the Milky Way: rejection sampling on the band mask
    placed = 0
    temps = np.array([[0.65, 0.78, 1.00], [0.85, 0.90, 1.00], [1.00, 1.00, 1.00],
                      [1.00, 0.92, 0.75], [1.00, 0.78, 0.55]])
    while placed < n_stars:
        # uniform on the sphere
        z = rng.uniform(-0.05, 1)       # (mostly) above the horizon
        phi = rng.uniform(0, 2 * np.pi)
        r = np.sqrt(1 - z * z)
        dirv = np.array([r * np.cos(phi), z, r * np.sin(phi)])
        if rng.random() > 0.45 + 0.55 * np.exp(-((dirv @ n) / 0.25) ** 2):
            continue
        elv = np.arcsin(z)
        x = phi / (2 * np.pi) * W
        y = (0.5 - elv / np.pi) * H
        mag = rng.random() ** 7                  # few bright, many faint
        color = temps[rng.integers(len(temps))] * (0.18 + 1.8 * mag)
        draw_star(x, y, color, 0.0 + 1.3 * mag)
        placed += 1
    # A handful of really bright stars with a soft halo
    for _ in range(40):
        z = rng.uniform(0.05, 1)
        phi = rng.uniform(0, 2 * np.pi)
        elv = np.arcsin(z)
        x, y = phi / (2 * np.pi) * W, (0.5 - elv / np.pi) * H
        c = temps[rng.integers(len(temps))]
        draw_star(x, y, c * 1.3, 1.1)
        draw_star(x, y, c * 0.18, 4.5)

    # --- moon: lit disc with maria + soft glow
    cosang = d @ MOON_DIR
    ang = np.arccos(np.clip(cosang, -1, 1))
    glow = np.exp(-(ang / 0.22) ** 2) * 0.16 + np.exp(-(ang / 0.07) ** 2) * 0.22
    img += (np.array([0.55, 0.65, 0.90]) * glow[..., None])
    # Orthonormal frame around the moon direction
    up = np.array([0.0, 1.0, 0.0])
    ex = np.cross(up, MOON_DIR)
    ex /= np.linalg.norm(ex)
    ey = np.cross(MOON_DIR, ex)
    local = np.stack([d @ ex, d @ ey], axis=-1) / np.sin(MOON_RADIUS)
    rr = np.sqrt((local ** 2).sum(-1))
    disc = (rr < 1.0) & (cosang > 0)
    if disc.any():
        px = local[..., 0]
        py = local[..., 1]
        zc = np.sqrt(np.clip(1 - px * px - py * py, 0, 1))
        maria = smoothstep(0.50, 0.62, noise_map(2.2))     # dark patches
        craters = noise_map(0.8)
        shade = 0.55 + 0.45 * zc                           # limb darkening
        base = (0.93 - 0.38 * maria + 0.10 * (craters - 0.5)) * shade
        moon = np.stack([base * 1.00, base * 0.97, base * 0.88], axis=-1)
        edge = smoothstep(1.0, 0.97, rr)[..., None]
        img = np.where(disc[..., None], img * (1 - edge) + moon * 1.05 * edge, img)

    img = np.clip(img, 0, 1) ** (1 / 1.05)
    Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(
        os.path.join(OUT, "night_sky.jpg"), quality=95, subsampling=0)

    write_dome()
    with open(os.path.join(OUT, "sky.mtl"), "w") as fh:
        fh.write("newmtl sky\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\nillum 1\nmap_Kd night_sky.jpg\n")


def write_dome(rings=48, sides=96):
    lines = ["# generated by generate_sky.py", "mtllib sky.mtl", "o skydome"]
    vts, vs, vns = [], [], []
    for j in range(rings + 1):
        el = np.pi / 2 - np.pi * j / rings
        for i in range(sides + 1):
            phi = 2 * np.pi * i / sides
            dv = np.array([np.cos(el) * np.cos(phi), np.sin(el), np.cos(el) * np.sin(phi)])
            vs.append(dv * DOME_RADIUS)
            vns.append(-dv)
            vts.append((i / sides, 0.5 + el / np.pi))
    lines += ["v %.4f %.4f %.4f" % tuple(p) for p in vs]
    lines += ["vt %.5f %.5f" % t for t in vts]
    lines += ["vn %.4f %.4f %.4f" % tuple(p) for p in vns]
    lines += ["usemtl sky", "s 1"]
    for j in range(rings):
        for i in range(sides):
            a = j * (sides + 1) + i + 1
            b, c, e = a + 1, a + sides + 1, a + sides + 2
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a, a, a, c, c, c, b, b, b))
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (b, b, b, c, c, c, e, e, e))
    open(os.path.join(OUT, "skydome.obj"), "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
    print("done")
