#!/usr/bin/env python3
"""Makes the house modelled in Blender (source/Casa_separada.obj, one object per
piece) ready for the game: house.obj (all but the door), house_door.obj (the
door and its knob, origin on its hinge) and house.mtl.

The Blender model is not in metres: its bed is 5.8 m long and its doorway 2.5
wide. SCALE (0.42) makes the doorway 1.07 m wide and 1.94 m high (the penguin
is 0.8 m wide and 1.8 m high), the bed 2.4 m and the rooms 3.9 m high. The
origin goes to the ground (the bottom of the porch's concrete slab) in the
middle of the footprint (house + porch along x); the concrete block under the
house goes into the ground. The house is the block on -x, its door on +x opens
onto the porch (+x), up four steps, under a roof on eight beams.

The export came without its .mtl: each Blender object gets a material here
(PIECES) and each material a flat colour (MATERIALS).

The door turns about its hinge, the doorway's edge at +z on the wall's outer
face (HINGE): it opens outwards, onto the porch. src/entities/House.cpp works in
Blender units with SCALE, GROUND_Y, CENTRE_X and HINGE: keep them in step.

Usage: python3 prepare_house.py        (writes next to this script)
Needs numpy.
"""
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "source", "Casa_separada.obj")
SCALE = 0.42
GROUND_Y = 0.69                   # the bottom of the porch slab (Blender units)
CENTRE_X = (-5.11 + 23.73) / 2    # the middle of the house and the porch along x
HINGE = (5.13, 1.35)              # (x, z) of the door's hinge (Blender units)
DOOR_PIECES = {"Cube.004", "Cylinder"}

# name: (diffuse colour, specular)
MATERIALS = {
    "Paredes": ((0.86, 0.84, 0.79), 0.05),    # the walls and the ceiling
    "Zocalo": ((0.48, 0.47, 0.45), 0.02),     # the concrete block the house stands on
    "Suelo": ((0.50, 0.38, 0.27), 0.10),      # the floor inside (the block's top)
    "hormigon": ((0.62, 0.61, 0.58), 0.02),   # the porch slab
    "Escalera": ((0.55, 0.54, 0.51), 0.02),
    "Viga": ((0.33, 0.24, 0.17), 0.05),       # the porch beams (wood)
    "Tejado": ((0.42, 0.44, 0.46), 0.25),     # the porch roof (metal sheet)
    "Puerta": ((0.45, 0.30, 0.18), 0.08),
    "Pomo": ((0.75, 0.70, 0.55), 0.6),
    "Cama": ((0.55, 0.20, 0.18), 0.03),
    "Aire": ((0.82, 0.83, 0.84), 0.3),        # the air conditioner (inside and out)
}
# Blender object: material
PIECES = {
    "Cube": "Paredes",
    "Cube.001": "Escalera",
    "Cube.002": "Aire",       # the air conditioner's inside unit, high on the -x wall
    "Cube.003": "Viga",
    "Cube.004": "Puerta",
    "Cube.005": "Aire",       # its outside unit
    "Cube.006": "Zocalo",
    "Cube.007": "hormigon",
    "Cube.009": "Cama",
    "Cylinder": "Pomo",
    "Plane": "Tejado",
    "Plane.001": "Tejado",
}


def material_of(piece, points):
    name = PIECES.get(piece, "Paredes")
    if name == "Zocalo" and points[:, 1].min() > 3.3:
        return "Suelo"  # (the block's top, over the house's own floor: what one walks on inside)
    return name


def write_obj(path, header, verts, uvs, normals, faces):
    """faces: (material, [(v, vt, vn)]) with 1-based indices into the full lists; keeps only the used ones"""
    used_v, used_t, used_n = {}, {}, {}
    for _, corners in faces:
        for v, t, n in corners:
            used_v.setdefault(v, len(used_v) + 1)
            if t:
                used_t.setdefault(t, len(used_t) + 1)
            if n:
                used_n.setdefault(n, len(used_n) + 1)
    with open(path, "w") as f:
        f.write(header + "mtllib house.mtl\n")
        for v in used_v:
            x, y, z = verts[v - 1]
            f.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
        for t in used_t:
            f.write("vt %s %s\n" % tuple(uvs[t - 1]))
        for n in used_n:
            f.write("vn %s %s %s\n" % tuple(normals[n - 1]))
        current = None
        for name, corners in sorted(faces, key=lambda fc: fc[0]):
            if name != current:
                f.write(f"usemtl {name}\n")
                current = name
            f.write("f " + " ".join("%d/%s/%s" % (used_v[v], used_t.get(t, ""), used_n.get(n, ""))
                                    for v, t, n in corners) + "\n")


def main():
    verts, normals, uvs = [], [], []
    house, door = [], []
    piece = None
    for line in open(SOURCE):
        p = line.split()
        if not p:
            continue
        if p[0] == "v":
            verts.append([float(x) for x in p[1:4]])
        elif p[0] == "vn":
            normals.append(p[1:4])
        elif p[0] == "vt":
            uvs.append(p[1:3])
        elif p[0] == "o":
            piece = p[1]
        elif p[0] == "f":
            corners = []
            for c in p[1:]:
                parts = (c.split("/") + ["", ""])[:3]
                corners.append(tuple(int(x) if x else 0 for x in parts))
            points = np.array([verts[c[0] - 1] for c in corners])
            (door if piece in DOOR_PIECES else house).append((material_of(piece, points), corners))
    v = np.array(verts)
    in_house = (v - np.array([CENTRE_X, GROUND_Y, 0.0])) * SCALE
    on_hinge = (v - np.array([HINGE[0], GROUND_Y, HINGE[1]])) * SCALE  # (the door: y as the house's)

    with open(os.path.join(HERE, "house.mtl"), "w") as f:
        f.write("# Made by prepare_house.py\n")
        for name, (kd, ks) in MATERIALS.items():
            f.write(f"\nnewmtl {name}\nKa 0 0 0\nKd {kd[0]} {kd[1]} {kd[2]}\nKs {ks} {ks} {ks}\nNs 40\nd 1\n")
    write_obj(os.path.join(HERE, "house.obj"), "# Made by prepare_house.py from source/Casa_separada.obj\n",
              in_house, uvs, normals, house)
    write_obj(os.path.join(HERE, "house_door.obj"),
              "# Made by prepare_house.py: the house's door, origin on its hinge (shut: it lies along -z)\n",
              on_hinge, uvs, normals, door)

    print("house: %d faces, door: %d faces" % (len(house), len(door)))
    print("bounds:", in_house.min(0).round(3), in_house.max(0).round(3))
    print("hinge at", ((HINGE[0] - CENTRE_X) * SCALE, 0.0, HINGE[1] * SCALE))


if __name__ == "__main__":
    main()
