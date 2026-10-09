"""Generates beer_can.obj (+ beer_can.mtl): the can of beer the penguin holds in his right flipper while
he drives the RV (Walker::setHeldModel, posed by SeatedPose).

A 33 cl can, a little bigger than a real one so that it reads from a few metres away (SCALE): a lathed
aluminium body with a domed base, a necked shoulder and a rolled lid rim, painted in bands (red, a cream
band with a thin gold stripe on each side, red), a silver lid and a ring pull on it.

In the CAN's frame: the origin in the middle of the can (where the flipper holds it), +Y along its axis
towards the lid, the ring pull on the +Z side of the lid. Metres. Smooth normals (written); the painted
bands are open rings that meet edge to edge, and together with the base and the lid close the can.
"""
import math

SCALE = 1.25          # times a real can (6.6 cm wide, 12.2 cm tall)
SEG = 32
R = 0.033 * SCALE     # body radius
H = 0.122 * SCALE     # height
Y0 = -H / 2           # the base

parts = []  # (material, verts, normals, faces)


def lathe(mat, prof, seg=SEG):
    """A band of a solid of revolution about Y from a (radius, y) profile, with smooth normals (from the
    profile's own slope). A profile point with radius 0 closes it there with a fan."""
    verts, normals, faces, rings = [], [], [], []
    n = len(prof)
    for i, (r, y) in enumerate(prof):
        # the normal of the profile at this point: perpendicular to the slope around it
        a, b = prof[max(i - 1, 0)], prof[min(i + 1, n - 1)]
        dr, dy = b[0] - a[0], b[1] - a[1]
        nr, ny = dy, -dr
        length = math.hypot(nr, ny) or 1.0
        nr, ny = nr / length, ny / length
        ring = []
        for k in range(seg if r > 0 else 1):
            t = 2 * math.pi * k / seg
            ring.append(len(verts))
            verts.append((r * math.cos(t), y, r * math.sin(t)))
            normals.append((nr * math.cos(t), ny, nr * math.sin(t)) if r > 0 else (0.0, 1.0 if ny > 0 else -1.0, 0.0))
        rings.append(ring)
    for i in range(n - 1):
        a, b = rings[i], rings[i + 1]
        for k in range(seg):
            m = (k + 1) % seg
            if len(a) == 1:
                faces.append([a[0], b[m], b[k]])
            elif len(b) == 1:
                faces.append([a[k], a[m], b[0]])
            else:
                faces.append([a[k], a[m], b[m], b[k]])
    # (fix_winding turns the faces outwards, by the normals, when the file is written)
    parts.append((mat, verts, normals, faces))


# the profile of the whole can, bottom to top, then cut into painted bands
base = [(0.0, Y0 + 0.010 * SCALE),            # the dome pushed in
        (R * 0.62, Y0 + 0.004 * SCALE),
        (R * 0.80, Y0),                       # the ring it stands on
        (R * 0.93, Y0 + 0.004 * SCALE),
        (R, Y0 + 0.012 * SCALE)]
lathe('alu', base)
bands = [('red', Y0 + 0.012 * SCALE, Y0 + 0.050 * SCALE),
         ('gold', Y0 + 0.050 * SCALE, Y0 + 0.053 * SCALE),
         ('cream', Y0 + 0.053 * SCALE, Y0 + 0.077 * SCALE),
         ('gold', Y0 + 0.077 * SCALE, Y0 + 0.080 * SCALE),
         ('red', Y0 + 0.080 * SCALE, Y0 + 0.108 * SCALE)]
for mat, y0, y1 in bands:
    lathe(mat, [(R, y0), (R, y1)])
lid_r = R * 0.82
lathe('alu', [(R, Y0 + 0.108 * SCALE),        # the shoulder, necking in
              (R * 0.95, Y0 + 0.114 * SCALE),
              (lid_r + 0.0015, Y0 + 0.119 * SCALE),
              (lid_r + 0.0025, Y0 + H),        # the rolled rim
              (lid_r, Y0 + H),
              (lid_r - 0.001, Y0 + H - 0.004 * SCALE)])
lathe('lid', [(lid_r - 0.001, Y0 + H - 0.004 * SCALE), (0.0, Y0 + H - 0.004 * SCALE)])


def slab(mat, x0, x1, y0, y1, z0, z1):
    """A box with flat normals (each face its own vertices)."""
    verts, normals, faces = [], [], []
    for axis in range(3):
        for sign in (-1, 1):
            lo, hi = [x0, y0, z0], [x1, y1, z1]
            corners = []
            o = [a for a in range(3) if a != axis]
            for u, v in ((0, 0), (1, 0), (1, 1), (0, 1)):
                p = [0.0] * 3
                p[axis] = hi[axis] if sign > 0 else lo[axis]
                p[o[0]] = (lo, hi)[u][o[0]]
                p[o[1]] = (lo, hi)[v][o[1]]
                corners.append(tuple(p))
            nrm = [0.0] * 3
            nrm[axis] = float(sign)
            base_index = len(verts)
            verts += corners
            normals += [tuple(nrm)] * 4
            f = [base_index, base_index + 1, base_index + 2, base_index + 3]
            # make the winding agree with the normal
            e1 = [corners[1][i] - corners[0][i] for i in range(3)]
            e2 = [corners[2][i] - corners[0][i] for i in range(3)]
            c = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
            if sum(c[i] * nrm[i] for i in range(3)) < 0:
                f = f[::-1]
            faces.append(f)
    parts.append((mat, verts, normals, faces))


# the ring pull: a flat tab lying on the lid, from the middle towards +z
top = Y0 + H - 0.004 * SCALE
slab('tab', -0.006 * SCALE, 0.006 * SCALE, top, top + 0.0015, -0.004 * SCALE, 0.016 * SCALE)
slab('tab', -0.0015, 0.0015, top, top + 0.003, -0.002 * SCALE, 0.0)   # the rivet


def fix_winding(mat, verts, normals, faces):
    """Turns every face so that its geometric normal agrees with the vertex normals."""
    out = []
    for f in faces:
        a, b, c = verts[f[0]], verts[f[1]], verts[f[2]]
        e1 = [b[i] - a[i] for i in range(3)]
        e2 = [c[i] - a[i] for i in range(3)]
        cr = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        nrm = [sum(normals[i][k] for i in f) for k in range(3)]
        out.append(f if sum(cr[k] * nrm[k] for k in range(3)) >= 0 else f[::-1])
    return out


MATS = {'red': ((0.62, 0.05, 0.06), 0.5), 'cream': ((0.93, 0.88, 0.72), 0.4), 'gold': ((0.80, 0.62, 0.20), 0.7),
        'alu': ((0.72, 0.73, 0.75), 0.8), 'lid': ((0.80, 0.80, 0.82), 0.8), 'tab': ((0.66, 0.67, 0.70), 0.7)}
with open('beer_can.mtl', 'w') as f:
    f.write('# generated by generate_beer_can.py\n')
    for n, (c, sp) in MATS.items():
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\n'
                f'Ks {sp} {sp} {sp}\nNs 60\nillum 2\n\n')
with open('beer_can.obj', 'w') as f:
    f.write('# generated by generate_beer_can.py\nmtllib beer_can.mtl\no beer_can\n')
    off = 0
    for cnt, (mat, verts, normals, faces) in enumerate(parts):
        f.write(f'g part{cnt}\nusemtl {mat}\n')
        for v in verts:
            f.write('v %.5f %.5f %.5f\n' % v)
        for n in normals:
            f.write('vn %.4f %.4f %.4f\n' % n)
        for fc in fix_winding(mat, verts, normals, faces):
            f.write('f ' + ' '.join(f'{i + 1 + off}//{i + 1 + off}' for i in fc) + '\n')
        off += len(verts)
print(len(parts), 'parts,', off, 'vertices')
