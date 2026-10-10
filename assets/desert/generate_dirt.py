#!/usr/bin/env python3
"""Generates the dirt textures of the map editor: dirt.jpg (packed earth, tileable, what the paint
tool lays down) and dirt_road.png (a dirt road: u across it, v along it, two wheel ruts, grass and
stones at the edges, an uneven edge cut out by the alpha channel).
Usage: python3 generate_dirt.py (writes next to this script; needs numpy and Pillow)."""
import os
import numpy as np
from PIL import Image
from generate_assets import fbm, lerp_color, periodic_noise, smoothstep, save_jpg, OUT


def earth(n, rng):
    tone = fbm(n, rng, 0.8, 2.2, 6)
    micro = fbm(n, rng, 0.2, 1.0, 3)
    t = 0.55 * tone + 0.45 * micro
    col = lerp_color(t, (112, 84, 58), (166, 132, 94))
    drift = fbm(n, rng, 2.0, 2.6, 2) - 0.5
    col = col + drift[..., None] * np.array([12, 4, -10])
    pebbles = (rng.random((n, n)) > 0.996) * 45.0
    grain = (periodic_noise(n, 0.0, rng) - 0.5) * 16
    return col + (grain + pebbles)[..., None]


def make_dirt(rng, n=512):
    save_jpg(earth(n, rng), "dirt.jpg")


def make_dirt_road(rng, w=512, h=1024):
    """h rows along the road, w columns across it. Tiles along v."""
    base = earth(h, rng)[:, :h]                      # square tile, cut to the width below
    col = base[:, :w].astype(float)
    u = (np.arange(w)[None, :] + 0.5) / w
    v = np.arange(h)[:, None] / h
    bump = fbm(h, rng, 1.2, 2.2, 4)[:, :w]
    # two wheel ruts: darker, smoother, a little hollow, with a ridge of loose earth between and
    # on the outsides
    ruts = np.zeros((h, w))
    for centre in (0.32, 0.68):
        wander = (bump - 0.5) * 0.05
        d = np.abs(u - centre - wander)
        ruts = np.maximum(ruts, 1 - smoothstep(0.035, 0.085, d))
    col = col * (1 - 0.17 * ruts[..., None])
    ridge = (np.exp(-((u - 0.5) / 0.07) ** 2)) * (0.4 + 0.6 * bump)
    col = col + (ridge * 14)[..., None] * np.array([1.0, 0.9, 0.7])
    # the edge: grass tufts and stones fading into an uneven cut-out
    edge = np.minimum(u, 1 - u)                       # 0 at the sides, 0.5 in the middle
    ragged = 0.07 + 0.09 * (fbm(h, rng, 0.9, 1.8, 5)[:, :w] - 0.4) * 2.0
    inside = smoothstep(ragged - 0.012, ragged + 0.012, edge)
    grass = (1 - smoothstep(ragged + 0.01, ragged + 0.16, edge)) * (0.4 + 0.6 * bump)
    green = np.array([92, 98, 54])
    col = col * (1 - 0.7 * grass[..., None]) + green * (0.7 * grass[..., None])
    stones = (rng.random((h, w)) > 0.997) * 40.0
    col = col + stones[..., None]
    rgba = np.dstack([np.clip(col, 0, 255), inside * 255]).astype(np.uint8)
    Image.fromarray(rgba, "RGBA").save(os.path.join(OUT, "dirt_road.png"))


if __name__ == "__main__":
    make_dirt(np.random.default_rng(21))
    make_dirt_road(np.random.default_rng(22))
    print("done")
