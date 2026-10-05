"""Generates folla_culos_run.glb: the creature (generate_folla_culos.py) rigged and with a looping
animation "run": it gallops on four legs, hands and feet on the ground.

glTF 2.0 binary (the engine's Assimp and f3d read it): one skin with the skeleton below, smooth
skinning (up to 4 bones per vertex, weights from the distance to each bone), and one animation of
24 frames per cycle. The bind pose is the T-pose of folla_culos.obj, so the same file is also the
rigged T-pose model. Animation 0 is "run" and animation 1 is "splat" (spread out against a surface,
breathing; see splat_pose). Check it with f3d (--animation-time) and see check() at the end.

Skeleton (bind = T-pose, joints at the points below; the hierarchy):
  pelvis > spine1 > spine2 > chest > neck > head
  chest > upperarm_L/R > forearm_L/R > hand_L/R           pelvis > thigh_L/R > shin_L/R > foot_L/R
Only the pelvis moves (it carries the body up and down); the animation is "in place": the feet
slide back under the body at the speed given by SPEED, so the engine moves the creature forward by
that much for the feet not to slip.

How the gait is built: each hand and foot follows a path in the body's frame (on the ground it goes
backwards at SPEED, in the air it swings forwards in an arc) and a two-bone IK puts the elbow/knee
where it has to be. A rotary gallop: the hands land one after the other, then the feet.
Needs numpy. Run it in this directory.
"""
import json
import math
import struct

import numpy as np

import generate_folla_culos as creature      # (it also writes the T-pose obj again: harmless)

# ------------------------------------------------------------------------- the cycle
CYCLE = 0.55            # seconds of one stride
FRAMES = 24             # samples per cycle
STRIDE_H, STANCE_H = 1.05, 0.44    # hands: how far each one travels on the ground, fraction of the cycle on it
STRIDE_F, STANCE_F = 1.30, 0.545   # feet
SPEED = STRIDE_H / (STANCE_H * CYCLE)                   # m/s: the ground goes by under the body
assert abs(SPEED - STRIDE_F / (STANCE_F * CYCLE)) < 0.02, 'hands and feet must agree on the speed'
PHASE = {'hand_L': 0.00, 'hand_R': 0.10, 'foot_L': 0.50, 'foot_R': 0.62}
LIFT_H, LIFT_F = 0.30, 0.24        # how high the hand / foot goes in the swing (m)

H0 = 0.80                          # height of the pelvis
SPINE_PITCH = (56.0, 61.0, 66.0, 71.0)   # pelvis, spine1, spine2, chest: leaning forward (degrees)
NECK_PITCH, HEAD_PITCH = 40.0, 6.0
HAND_X, FOOT_X = 0.36, 0.20        # sideways distance of the contacts from the middle
HAND_Z, FOOT_Z = 0.98, -0.02       # where the middle of each contact path is (z)
ARM_POLE = np.array([0.55, 0.8, -0.35])   # (for the right arm) where the elbows point: out, up and back
LEG_POLE = np.array([0.0, 0.25, 1.0])     # the knees point forward

GLOWING = {'eye': (1.0, 0.86, 0.06)}    # materials that glow (emissive): the eyes, bright yellow

# ------------------------------------------------------------------------- skeleton
J = {  # name: (parent, bind position)
    'pelvis': (None, (0.0, 1.05, 0.0)), 'spine1': ('pelvis', (0.0, 1.25, 0.0)),
    'spine2': ('spine1', (0.0, 1.55, 0.0)), 'chest': ('spine2', (0.0, 1.80, 0.0)),
    'neck': ('chest', (0.0, 1.93, 0.0)), 'head': ('neck', (0.0, 2.05, 0.02)),
}
for s, n in ((-1, 'R'), (1, 'L')):   # the creature faces +z: its left side is +x
    J['upperarm_' + n] = ('chest', (s * 0.255, 1.84, 0.0))
    J['forearm_' + n] = ('upperarm_' + n, (s * 0.82, 1.84, -0.02))
    J['hand_' + n] = ('forearm_' + n, (s * 1.38, 1.84, 0.0))
    J['thigh_' + n] = ('pelvis', (s * 0.115, 1.14, 0.0))
    J['shin_' + n] = ('thigh_' + n, (s * 0.150, 0.60, 0.03))
    J['foot_' + n] = ('shin_' + n, (s * 0.170, 0.10, -0.012))
ORDER = list(J)            # parents come before their children
INDEX = {n: i for i, n in enumerate(ORDER)}
BIND = {n: np.array(J[n][1], float) for n in ORDER}
# the end of each bone (for the skin weights: the segment from its joint to its end)
END = {'pelvis': 'spine1', 'spine1': 'spine2', 'spine2': 'chest', 'chest': 'neck', 'neck': 'head'}
ENDPOINT = {n: BIND[END[n]] for n in END}
ENDPOINT['head'] = np.array([0.0, 2.33, 0.03])
for n in 'LR':
    s = 1 if n == 'L' else -1
    ENDPOINT['upperarm_' + n] = BIND['forearm_' + n]
    ENDPOINT['forearm_' + n] = BIND['hand_' + n]
    ENDPOINT['hand_' + n] = np.array([s * 1.80, 1.84, 0.0])
    ENDPOINT['thigh_' + n] = BIND['shin_' + n]
    ENDPOINT['shin_' + n] = BIND['foot_' + n]
    ENDPOINT['foot_' + n] = np.array([s * 0.172, 0.04, 0.35])


# ------------------------------------------------------------------------- the skin
def segment_distance(P, a, b):
    ab = b - a
    t = np.clip(((P - a) @ ab) / (ab @ ab), 0.0, 1.0)
    return np.linalg.norm(P - (a + t[:, None] * ab), axis=1)


def skin_weights(P):
    """(n, 4) bone indices and weights: the nearest bones, nearer = heavier."""
    d = np.stack([segment_distance(P, BIND[n], ENDPOINT[n]) for n in ORDER], axis=1)
    w = 1.0 / (d + 0.015) ** 4
    idx = np.argsort(-w, axis=1)[:, :4]
    top = np.take_along_axis(w, idx, axis=1)
    top[top < 0.02 * top[:, :1]] = 0.0
    top /= top.sum(axis=1, keepdims=True)
    return idx, top


# ------------------------------------------------------------------------- maths
def Rx(deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]], float)


def Ry(deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]], float)


def unit(v):
    return v / np.linalg.norm(v)


def frame(d, up):
    d = unit(np.asarray(d, float))
    u = np.asarray(up, float) - d * np.dot(up, d)
    u = unit(u)
    return np.stack([d, u, np.cross(d, u)], axis=1)      # columns: along, up, side


def look(d0, up0, d1, up1):
    """The rotation taking the bone frame (d0, up0) to (d1, up1)."""
    return frame(d1, up1) @ frame(d0, up0).T


def two_bone(A, C, l1, l2, pole):
    """The joint of a two-bone chain from A to (as near as it can to) C: returns it and the end."""
    v = C - A
    d = np.linalg.norm(v)
    d = min(max(d, abs(l1 - l2) + 1e-3), l1 + l2 - 1e-3)
    a = unit(v)
    x = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
    h = math.sqrt(max(l1 * l1 - x * x, 0.0))
    pp = pole - a * np.dot(pole, a)
    pp = unit(pp)
    return A + a * x + pp * h, A + a * d, pp


def smooth(t):
    return t * t * (3 - 2 * t)


def quaternion(R):
    m = R
    tr = m[0, 0] + m[1, 1] + m[2, 2]
    if tr > 0:
        s = math.sqrt(tr + 1) * 2
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = math.sqrt(1 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        q = [0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s]
    elif m[1, 1] > m[2, 2]:
        s = math.sqrt(1 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        q = [(m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s]
    else:
        s = math.sqrt(1 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        q = [(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s, (m[1, 0] - m[0, 1]) / s]
    q = np.array(q)
    return q / np.linalg.norm(q)


# ------------------------------------------------------------------------- the mesh
verts, norms, mats = [], [], []
tris = {}
base = 0
for mat, V, N, F in creature.pieces:
    verts.append(V); norms.append(N)
    for f in F:
        for i in range(1, len(f) - 1):
            tris.setdefault(mat, []).append((f[0] + base, f[i] + base, f[i + 1] + base))
    base += len(V)
VERT = np.vstack(verts)
NORM = np.vstack(norms)
JOINTS, WEIGHTS = skin_weights(VERT)
print('mesh:', len(VERT), 'vertices;', sum(len(t) for t in tris.values()), 'triangles')

# the vertices that touch the ground: the claws of the hands and the soles of the feet
def limb_vertices(name):
    sel = (JOINTS == INDEX[name]) * WEIGHTS
    return np.where(sel.sum(axis=1) > 0.5)[0]


HAND_V = {n: limb_vertices('hand_' + n) for n in 'LR'}
FOOT_V = {n: limb_vertices('foot_' + n) for n in 'LR'}


# ------------------------------------------------------------------------- the pose
def contact_path(phase, stride, stance, lift, t, z0):
    """(z, lift above the ground, grounded) of a hand or foot at time t (0..1 of the cycle)."""
    p = (t - phase) % 1.0
    if p < stance:                       # on the ground: it goes back under the body
        u = p / stance
        return z0 + stride * (0.5 - u), 0.0, True
    w = (p - stance) / (1.0 - stance)     # in the air: it swings forward
    z = z0 - stride / 2 + stride * smooth(w)
    return z, lift * math.sin(math.pi * w) ** 0.8, False


def pose(t):
    """World rotation and position of every bone at time t (0..1): {name: (R, p)}."""
    R, P = {}, {}
    ph = 2 * math.pi * t
    # the spine: leaning forward, gathering and stretching once per stride
    flex = 6.0 * math.sin(ph + 0.4)
    pitch = [SPINE_PITCH[0] + flex, SPINE_PITCH[1] + 0.6 * flex, SPINE_PITCH[2] + 0.2 * flex,
             SPINE_PITCH[3] - 0.5 * flex]
    bob = 0.05 * math.cos(2 * ph + 0.9)
    R['pelvis'], P['pelvis'] = Rx(pitch[0]), np.array([0.0, H0 + bob, 0.0])
    for name, parent, ang in (('spine1', 'pelvis', pitch[1]), ('spine2', 'spine1', pitch[2]),
                              ('chest', 'spine2', pitch[3]), ('neck', 'chest', NECK_PITCH + 0.5 * flex),
                              ('head', 'neck', HEAD_PITCH - 0.7 * flex + 4 * math.sin(2 * ph))):
        R[name] = Rx(ang)
    for name in ORDER:
        parent = J[name][0]
        if parent is None:
            continue
        # a child's position: its parent's position plus the bind offset rotated by the parent
        P[name] = None   # (filled below, after the rotations that the IK needs)

    def place(name):
        parent = J[name][0]
        P[name] = P[parent] + R[parent] @ (BIND[name] - BIND[parent])

    for name in ('spine1', 'spine2', 'chest', 'neck', 'head'):
        place(name)

    for side in 'LR':
        s = 1 if side == 'L' else -1
        # ---- the arm
        place('upperarm_' + side)
        z, up, grounded = contact_path(PHASE['hand_' + side], STRIDE_H, STANCE_H, LIFT_H, t, HAND_Z)
        psi = (22.0 if grounded else 22.0 - 55.0 * math.sin(math.pi * ((t - PHASE['hand_' + side]) % 1.0 - STANCE_H) / (1 - STANCE_H)))
        Rh = Rx(psi) @ Ry(-90.0 * s)             # fingers forward, palm down, then pitched
        low = (BIND_VERTS_HAND[side] @ Rh.T)[:, 1].min()
        target = np.array([s * HAND_X, -low + up, z])
        A = P['upperarm_' + side]
        l1 = np.linalg.norm(BIND['forearm_' + side] - BIND['upperarm_' + side])
        l2 = np.linalg.norm(BIND['hand_' + side] - BIND['forearm_' + side])
        pole = np.array([ARM_POLE[0] * s, ARM_POLE[1], ARM_POLE[2]])
        E, W, pp = two_bone(A, target, l1, l2, pole)
        up0 = np.array([0.0, -1.0, 0.0])
        R['upperarm_' + side] = look(BIND['forearm_' + side] - BIND['upperarm_' + side], up0, E - A, -pp)
        R['forearm_' + side] = look(BIND['hand_' + side] - BIND['forearm_' + side], up0, W - E, -pp)
        R['hand_' + side] = Rh
        P['forearm_' + side], P['hand_' + side] = E, W
        # ---- the leg
        place('thigh_' + side)
        z, up, grounded = contact_path(PHASE['foot_' + side], STRIDE_F, STANCE_F, LIFT_F, t, FOOT_Z)
        q = (t - PHASE['foot_' + side]) % 1.0
        if grounded:
            psi = -10.0 * (1 - 2 * q / STANCE_F) * -1.0          # heel first, then the toes push off
            psi = 8.0 * (2 * q / STANCE_F - 1.0)
        else:
            psi = 28.0 * math.sin(math.pi * (q - STANCE_F) / (1 - STANCE_F))
        Rf = Rx(psi)
        low = (BIND_VERTS_FOOT[side] @ Rf.T)[:, 1].min()
        target = np.array([s * FOOT_X, -low + up, z])
        A = P['thigh_' + side]
        l1 = np.linalg.norm(BIND['shin_' + side] - BIND['thigh_' + side])
        l2 = np.linalg.norm(BIND['foot_' + side] - BIND['shin_' + side])
        E, W, pp = two_bone(A, target, l1, l2, LEG_POLE)
        up0 = np.array([0.0, 0.0, -1.0])
        R['thigh_' + side] = look(BIND['shin_' + side] - BIND['thigh_' + side], up0, E - A, -pp)
        R['shin_' + side] = look(BIND['foot_' + side] - BIND['shin_' + side], up0, W - E, -pp)
        R['foot_' + side] = Rf
        P['shin_' + side], P['foot_' + side] = E, W
    return R, P


# the bind-pose vertices of the hands and feet, relative to their wrist / ankle
BIND_VERTS_HAND = {n: VERT[HAND_V[n]] - BIND['hand_' + n] for n in 'LR'}
BIND_VERTS_FOOT = {n: VERT[FOOT_V[n]] - BIND['foot_' + n] for n in 'LR'}


def skinned(R, P):
    """The mesh in the pose (linear blend skinning)."""
    out = np.zeros_like(VERT)
    nrm = np.zeros_like(NORM)
    for k in range(4):
        for bi, name in enumerate(ORDER):
            sel = (JOINTS[:, k] == bi) & (WEIGHTS[:, k] > 0)
            if not sel.any():
                continue
            w = WEIGHTS[sel, k][:, None]
            out[sel] += w * ((VERT[sel] - BIND[name]) @ R[name].T + P[name])
            nrm[sel] += w * (NORM[sel] @ R[name].T)
    return out, nrm


# ------------------------------------------------------------------------- the glb
def rz(deg):
    a = math.radians(deg); c, s_ = math.cos(a), math.sin(a)
    return np.array([[c, -s_, 0], [s_, c, 0], [0, 0, 1]], float)


# ---- the second animation: "splat". The creature spread out like a dead bug, flat against a surface
# (the windshield of the RV when it has been run over): it faces +z (the surface is in front of it,
# as if it had run into it), the arms and legs open out in the xy plane in a star, the head
# drooping to one side, and it breathes very slowly: the chest and the shoulders rise and fall.
SPLAT_PERIOD = 4.0      # seconds of one breath
SPLAT_FRAMES = 24


def splat_pose(t):
    R, P = {}, {}
    ph = 2 * math.pi * t
    breath = math.sin(ph)
    R['pelvis'] = rz(5.0)
    P['pelvis'] = BIND['pelvis'] + np.array([0.0, 0.012 * breath, 0.0])
    # the spine: a slight bend sideways and the chest swelling forwards and back with each breath
    R['spine1'] = rz(3.0) @ Rx(-1.2 * breath)
    R['spine2'] = rz(1.0) @ Rx(-2.2 * breath)
    R['chest'] = rz(-2.0) @ Rx(-3.0 * breath)
    R['neck'] = rz(8.0) @ Rx(2.0 * breath)
    R['head'] = rz(16.0 + 1.5 * math.sin(ph - 0.8)) @ Rx(-4.0)
    for name in ('spine1', 'spine2', 'chest', 'neck', 'head'):
        parent = J[name][0]
        P[name] = P[parent] + R[parent] @ (BIND[name] - BIND[parent])
    # (angle in the xy plane, from the bind direction: arms start along +-x, legs hang along -y)
    ARMS = {'L': (50.0, 66.0, 88.0), 'R': (36.0, 58.0, 74.0)}      # raised, like a bug's legs in the air
    LEGS = {'L': (52.0, 24.0, 12.0), 'R': (40.0, 6.0, -4.0)}       # frog legs: knees out, shins down
    for side in 'LR':
        s_ = 1 if side == 'L' else -1
        up, fore, hand = ARMS[side]
        sway = 2.5 * math.sin(ph - 0.5)                            # the shoulders rise with the breath
        R['upperarm_' + side] = rz(s_ * (up + sway))
        R['forearm_' + side] = rz(s_ * (fore + 0.6 * sway))
        R['hand_' + side] = rz(s_ * (hand + 0.3 * sway)) @ Rx(-14.0 * s_)   # fingers splayed
        th, sh, ft = LEGS[side]
        R['thigh_' + side] = rz(s_ * th)
        R['shin_' + side] = rz(s_ * sh)
        R['foot_' + side] = rz(s_ * ft) @ Rx(-20.0)
        for name in ('upperarm_', 'thigh_'):
            n = name + side
            P[n] = P[J[n][0]] + R[J[n][0]] @ (BIND[n] - BIND[J[n][0]])
        for name in ('forearm_', 'hand_'):
            n = name + side
            P[n] = P[J[n][0]] + R[J[n][0]] @ (BIND[n] - BIND[J[n][0]])
        for name in ('shin_', 'foot_'):
            n = name + side
            P[n] = P[J[n][0]] + R[J[n][0]] @ (BIND[n] - BIND[J[n][0]])
    return R, P


ANIMATIONS = [('run', CYCLE, FRAMES, pose), ('splat', SPLAT_PERIOD, SPLAT_FRAMES, splat_pose)]


def sample(period, frames, pose_fn):
    """(times, {bone: [quaternions]}, root positions, samples) of one animation."""
    times = [period * k / frames for k in range(frames + 1)]
    samples = [pose_fn((k % frames) / frames) for k in range(frames + 1)]
    rot = {n: [] for n in ORDER}
    for R, P in samples:
        for n in ORDER:
            parent = J[n][0]
            local = R[n] if parent is None else R[parent].T @ R[n]
            rot[n].append(quaternion(local))
    for n in ORDER:                                   # continuity: neighbouring quaternions on the same side
        for k in range(1, len(rot[n])):
            if np.dot(rot[n][k], rot[n][k - 1]) < 0:
                rot[n][k] = -rot[n][k]
    return times, rot, [P['pelvis'] for R, P in samples], samples


def build():
    sampled = [(name,) + sample(period, frames, fn) for name, period, frames, fn in ANIMATIONS]
    samples = sampled[0][4]            # the run, for check()
    blob = bytearray()
    views, accessors = [], []

    def add(data, ctype, count, atype, target=None, minmax=None):
        while len(blob) % 4:
            blob.append(0)
        views.append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(data), **({'target': target} if target else {})})
        blob.extend(data)
        acc = {'bufferView': len(views) - 1, 'componentType': ctype, 'count': count, 'type': atype}
        if minmax:
            acc['min'], acc['max'] = minmax
        accessors.append(acc)
        return len(accessors) - 1

    FLOAT, USHORT, UINT = 5126, 5123, 5125
    pos_acc = add(VERT.astype('<f4').tobytes(), FLOAT, len(VERT), 'VEC3', 34962,
                  (VERT.min(0).tolist(), VERT.max(0).tolist()))
    nrm_acc = add(NORM.astype('<f4').tobytes(), FLOAT, len(NORM), 'VEC3', 34962)
    jnt_acc = add(JOINTS.astype('<u2').tobytes(), USHORT, len(JOINTS), 'VEC4', 34962)
    wgt_acc = add(WEIGHTS.astype('<f4').tobytes(), FLOAT, len(WEIGHTS), 'VEC4', 34962)
    ibm = np.zeros((len(ORDER), 4, 4))
    for i, n in enumerate(ORDER):
        ibm[i] = np.eye(4)
        ibm[i][:3, 3] = -BIND[n]
    ibm_acc = add(ibm.transpose(0, 2, 1).astype('<f4').tobytes(), FLOAT, len(ORDER), 'MAT4')

    materials, primitives = [], []
    for name, (r, g, b) in creature.MATS.items():
        if name not in tris:
            continue
        idx = np.array(tris[name], dtype='<u4').reshape(-1)
        ia = add(idx.tobytes(), UINT, len(idx), 'SCALAR', 34963)
        # (the colours as they are, not converted to linear: the engine does no gamma correction)
        if name in GLOWING:
            r, g, b = GLOWING[name]
        materials.append({'name': name, 'pbrMetallicRoughness': {
            'baseColorFactor': [r, g, b, 1.0], 'metallicFactor': 0.0, 'roughnessFactor': 0.75},
            **({'emissiveFactor': list(GLOWING[name])} if name in GLOWING else {})})
        primitives.append({'attributes': {'POSITION': pos_acc, 'NORMAL': nrm_acc, 'JOINTS_0': jnt_acc, 'WEIGHTS_0': wgt_acc},
                           'indices': ia, 'material': len(materials) - 1})

    nodes = []
    for n in ORDER:
        node = {'name': n}
        parent = J[n][0]
        off = BIND[n] if parent is None else BIND[n] - BIND[parent]
        node['translation'] = [float(c) for c in off]
        nodes.append(node)
    for n in ORDER:
        kids = [INDEX[c] for c in ORDER if J[c][0] == n]
        if kids:
            nodes[INDEX[n]]['children'] = kids
    mesh_node = len(nodes)
    nodes.append({'name': 'folla_culos', 'mesh': 0, 'skin': 0})

    animations = []
    for name, times, rot, root_pos, _ in sampled:
        t_acc = add(np.array(times, '<f4').tobytes(), FLOAT, len(times), 'SCALAR', None, ([times[0]], [times[-1]]))
        samplers, channels = [], []
        for n in ORDER:
            a = add(np.array(rot[n], '<f4').tobytes(), FLOAT, len(times), 'VEC4')
            samplers.append({'input': t_acc, 'output': a, 'interpolation': 'LINEAR'})
            channels.append({'sampler': len(samplers) - 1, 'target': {'node': INDEX[n], 'path': 'rotation'}})
        a = add(np.array(root_pos, '<f4').tobytes(), FLOAT, len(times), 'VEC3')
        samplers.append({'input': t_acc, 'output': a, 'interpolation': 'LINEAR'})
        channels.append({'sampler': len(samplers) - 1, 'target': {'node': INDEX['pelvis'], 'path': 'translation'}})
        animations.append({'name': name, 'samplers': samplers, 'channels': channels})

    gltf = {'asset': {'version': '2.0', 'generator': 'generate_folla_culos_run.py'},
            'scene': 0, 'scenes': [{'nodes': [0, mesh_node]}], 'nodes': nodes,
            'meshes': [{'name': 'folla_culos', 'primitives': primitives}], 'materials': materials,
            'skins': [{'joints': list(range(len(ORDER))), 'inverseBindMatrices': ibm_acc, 'skeleton': 0}],
            'animations': animations,
            'buffers': [{'byteLength': len(blob)}], 'bufferViews': views, 'accessors': accessors}
    js = json.dumps(gltf, separators=(',', ':')).encode()
    js += b' ' * (-len(js) % 4)
    blob.extend(b'\0' * (-len(blob) % 4))
    with open('folla_culos_run.glb', 'wb') as f:
        f.write(struct.pack('<4sII', b'glTF', 2, 12 + 8 + len(js) + 8 + len(blob)))
        f.write(struct.pack('<I4s', len(js), b'JSON') + js)
        f.write(struct.pack('<I4s', len(blob), b'BIN\0') + bytes(blob))
    return samples


def check(samples):
    """Numbers to trust: the lowest vertex each frame, and whether the feet and hands slip: while a
    limb is down, its joint plus the distance the body has moved forward since it landed must stay put."""
    lows = [skinned(R, P)[0][:, 1].min() for R, P in samples[:-1]]
    print('lowest vertex over the cycle: %.3f .. %.3f m' % (min(lows), max(lows)))
    for limb, joint, stance in (('hand', 'hand_', STANCE_H), ('foot', 'foot_', STANCE_F)):
        for side in 'LR':
            zs, ys = [], []
            for k in range(FRAMES):
                t = k / FRAMES
                p = (t - PHASE[limb + '_' + side]) % 1.0      # time since it landed, in cycles
                if 0.01 < p < stance - 0.01:
                    P = samples[k][1]
                    zs.append(P[joint + side][2] + SPEED * p * CYCLE)
                    ys.append(P[joint + side][1])
            print('  %s %s: down for %d frames, slips %.3f m, joint height %.3f..%.3f' %
                  (limb, side, len(zs), max(zs) - min(zs), min(ys), max(ys)))


if __name__ == '__main__':
    samples = build()
    print('speed %.2f m/s, cycle %.2f s, %d frames' % (SPEED, CYCLE, FRAMES))
    check(samples)
