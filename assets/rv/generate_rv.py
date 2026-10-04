"""Generates rv.obj / rv.mtl: a retro RV. Y up, front of the vehicle toward +Z, units in meters."""
import math

W = 1.2  # half width
objs = []  # (material, verts, faces)

def cur(mat):
    objs.append((mat, [], []))
    return objs[-1]

def vol(verts, faces):
    s = 0
    for f in faces:
        a = verts[f[0]]
        for i in range(1, len(f) - 1):
            b, c = verts[f[i]], verts[f[i + 1]]
            s += (a[0]*(b[1]*c[2]-b[2]*c[1]) - a[1]*(b[0]*c[2]-b[2]*c[0]) + a[2]*(b[0]*c[1]-b[1]*c[0]))
    return s

def add(mat, verts, faces):
    if vol(verts, faces) < 0:
        faces = [f[::-1] for f in faces]
    objs.append((mat, verts, faces))

def hexa(mat, v):
    """v: 4 verts of one face (ccw-ish loop) then the 4 matching verts of the opposite face."""
    f = [[0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]]
    add(mat, list(v), f)

def box(mat, x0, x1, y0, y1, z0, z1):
    hexa(mat, [(x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1),
               (x0,y1,z0),(x1,y1,z0),(x1,y1,z1),(x0,y1,z1)])

def prism(mat, prof, x0, x1):
    n = len(prof)
    verts = [(x0, y, z) for z, y in prof] + [(x1, y, z) for z, y in prof]
    faces = [list(range(n)), list(range(2*n-1, n-1, -1))]
    for i in range(n):
        j = (i + 1) % n
        faces.append([i, j, n+j, n+i])
    add(mat, verts, faces)

def cyl(mat, cx, cy, cz, r, x0, x1, seg=24, ymin=None):
    verts, faces = [], []
    for x in (x0, x1):
        for i in range(seg):
            a = 2*math.pi*i/seg
            y = cy + r*math.sin(a)
            verts.append((x, y if ymin is None else max(y, ymin), cz + r*math.cos(a)))
    faces.append(list(range(seg)))
    faces.append(list(range(2*seg-1, seg-1, -1)))
    for i in range(seg):
        j = (i+1) % seg
        faces.append([i, j, seg+j, seg+i])
    add(mat, verts, faces)

# body profile (z, y): rear -> front, slanted windshield, nearly flat roof
PROF = [(-3.55,0.55),(3.55,0.55),(3.55,1.7),(3.0,2.95),(2.85,3.05),(-3.4,3.05),(-3.55,2.9)]
prism('body', PROF, -W, W)

def lerp(a, b, t): return tuple(a[i]+(b[i]-a[i])*t for i in range(len(a)))

# slanted front face from (3.55,1.7) to (3.0,2.95)
A, B = (3.55, 1.7), (3.0, 2.95)
nz, ny = 1.25, 0.55
l = math.hypot(nz, ny); nz, ny = nz/l, ny/l
def front(t, x, off=0.0):
    z, y = lerp(A, B, t)
    return (x, y + ny*off, z + nz*off)
# windshield: two panes
for xa, xb in ((-1.05, -0.03), (0.03, 1.05)):
    ta, tb = 0.18, 0.85
    d = 0.03
    hexa('glass', [front(ta,xa), front(ta,xb), front(tb,xb), front(tb,xa),
                   front(ta,xa,d), front(ta,xb,d), front(tb,xb,d), front(tb,xa,d)])
# front lower fascia: grille + headlights on the vertical front (z=3.5..3.55)
FZ = 3.55
box('grille', -0.85, -0.05, 0.85, 1.2, FZ-0.02, FZ+0.04)
box('grille', 0.05, 0.85, 0.85, 1.2, FZ-0.02, FZ+0.04)
for sx in (-1, 1):
    for y in (0.9, 1.08):
        cyl_x = sx*1.0
        # headlight: cylinder with axis along z, built by rotating cyl coordinates
        seg = 16; r = 0.075; verts = []; faces = []
        for z in (FZ, FZ+0.06):
            for i in range(seg):
                a = 2*math.pi*i/seg
                verts.append((cyl_x + r*math.cos(a), y + r*math.sin(a), z))
        faces.append(list(range(seg))); faces.append(list(range(2*seg-1, seg-1, -1)))
        for i in range(seg):
            j = (i+1) % seg; faces.append([i, j, seg+j, seg+i])
        add('light', verts, faces)
# bumper
box('bumper', -1.25, 1.25, 0.5, 0.75, 3.35, 3.7)
box('bumper', -1.25, 1.25, 0.5, 0.75, -3.7, -3.4)
# side details. Stripes are 0.02 thick, door/windows sit above them so nothing is coplanar.
for sx in (-1, 1):
    def sbox(mat, y0, y1, z0, z1, d0=0.0, d1=0.02):
        a, b = sx*(W+d0), sx*(W+d1)
        box(mat, min(a, b), max(a, b), y0, y1, z0, z1)
    sbox('orange', 2.55, 2.75, -3.55, 3.08)
    sbox('teal', 1.3, 1.4, -3.55, 3.55)
    sbox('orange', 1.2, 1.3, -3.55, 3.55)
    for z0, z1 in ((-3.1,-2.2),(-1.9,-1.0)):
        sbox('glass', 1.6, 2.35, z0, z1, 0.0, 0.03)
    sbox('glass', 1.9, 2.5, 0.45, 0.9, 0.0, 0.03)  # small cab-side window
    if sx > 0:
        # door only on the +X side
        sbox('door', 0.6, 2.45, -0.8, 0.1, 0.0, 0.04)
        sbox('glass', 1.6, 2.35, -0.65, -0.05, 0.04, 0.06)
        sbox('rack', 1.0, 1.08, -0.15, -0.05, 0.04, 0.09)  # handle
    else:
        sbox('glass', 1.6, 2.35, -0.7, 0.0, 0.0, 0.03)
    # mirror: arm + head
    sbox('mirror', 2.0, 2.06, 2.98, 3.02, 0.0, 0.2)
    sbox('mirror', 1.75, 2.3, 2.93, 3.07, 0.2, 0.3)
    # wheels: dark arch disc on the body side, tire straddling the wall, hub caps
    for cz in (2.4, -2.3):
        lo, hi = (W+0.01, W+0.02)
        cyl('arch', 0, 0.5, cz, 0.62, sx*lo if sx > 0 else sx*hi, sx*hi if sx > 0 else sx*lo, 28, ymin=0.56)
        a, b = sx*(W-0.2), sx*(W+0.18)
        cyl('tire', 0, 0.5, cz, 0.5, min(a, b), max(a, b), 28)
        a, b = sx*(W+0.18), sx*(W+0.2)
        cyl('hub', 0, 0.5, cz, 0.28, min(a, b), max(a, b), 20)
        a, b = sx*(W+0.2), sx*(W+0.22)
        cyl('cap', 0, 0.5, cz, 0.09, min(a, b), max(a, b), 12)
# rear: long window + ladder (stair) on the right
RZ = -3.55
box('glass', -1.0, 0.45, 1.6, 2.35, RZ-0.03, RZ)
box('orange', -W, W, 2.55, 2.75, RZ-0.02, RZ)
for x in (0.7, 1.0):
    box('rack', x-0.025, x+0.025, 0.85, 2.95, RZ-0.1, RZ-0.05)           # rails
    for y in (0.85, 2.9):
        box('rack', x-0.02, x+0.02, y, y+0.05, RZ-0.1, RZ)               # standoffs
for i in range(8):
    y = 1.0 + i*0.25
    box('rack', 0.7, 1.0, y, y+0.04, RZ-0.1, RZ-0.06)                    # rungs
# front orange stripe across the cab + teal
box('orange', -W, W, 1.2, 1.3, FZ, FZ+0.02)
box('teal', -W, W, 1.3, 1.4, FZ, FZ+0.02)
# roof rack (rear) and roof vent bar
for sx in (-1, 1):
    box('rack', sx*0.9-0.03, sx*0.9+0.03, 3.05, 3.3, -3.3, -3.24)
    box('rack', sx*0.9-0.03, sx*0.9+0.03, 3.05, 3.3, -0.5, -0.44)
box('rack', -0.93, 0.93, 3.27, 3.33, -3.3, -0.44)
box('rack', -0.45, 0.45, 3.05, 3.2, 0.3, 2.3)  # roof AC unit
MATS = {
 'body': (0.97, 0.92, 0.80), 'glass': (0.05, 0.30, 0.45),
 'orange': (1.00, 0.45, 0.05), 'teal': (0.05, 0.90, 0.65), 'grille': (0.20, 0.17, 0.12),
 'light': (1.00, 1.00, 1.00), 'bumper': (0.80, 0.55, 0.28), 'mirror': (0.60, 0.60, 0.62),
 'rack': (0.88, 0.84, 0.74), 'tire': (0.22, 0.20, 0.17), 'hub': (0.75, 0.62, 0.38),
 'door': (0.90, 0.84, 0.72), 'arch': (0.12, 0.11, 0.09), 'cap': (0.55, 0.45, 0.28),
}
with open('rv.mtl', 'w') as f:
    f.write('# generated by generate_rv.py\n')
    for n, c in MATS.items():
        f.write(f'newmtl {n}\nKa {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKd {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}\nKs 0.05 0.05 0.05\nNs 10\nillum 2\n\n')

with open('rv.obj', 'w') as f:
    f.write('# generated by generate_rv.py\nmtllib rv.mtl\no rv\n')
    off = 0; cnt = 0
    for mat, verts, faces in objs:
        f.write(f'g part{cnt}\nusemtl {mat}\n'); cnt += 1
        for v in verts: f.write('v %.5f %.5f %.5f\n' % v)
        for fc in faces: f.write('f ' + ' '.join(str(i+1+off) for i in fc) + '\n')
        off += len(verts)
print(cnt, 'parts,', off, 'vertices')
