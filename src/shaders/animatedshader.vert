#version 330 core

// --- Uniforms ---
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec3 objposition;
uniform mat4 objrotation;

// Bind pose fit (see AnimatedModel::computeFit)
uniform vec3 fitCenter;
uniform float fitScale;

// 0 when drawing plain (static) meshes with this shader
uniform int skinned;

// Procedural breathing for static meshes (the creature); 0 amplitude = off
uniform float breathAmp;
uniform float breathTime;

// Wind for static meshes (trees); 0 = off, 1 = a normal breeze. Needs the mesh
// to stand on y = 0 with its trunk on the y axis (see sway()).
uniform float swayAmp;

// 1 = show the skinned model in its idle pose (see idle()), 0 = skinned as usual
uniform int idlePose;

// Used by meshes that have no bone weights
uniform mat4 meshMat;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

// --- Inputs ---
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 aTexCoord;
// 12 influences per vertex, as three (ids, weights) groups of four
layout(location = 3) in ivec4 s_vIDs0;
layout(location = 4) in vec4 s_vWeights0;
layout(location = 5) in ivec4 s_vIDs1;
layout(location = 6) in vec4 s_vWeights1;
layout(location = 7) in ivec4 s_vIDs2;
layout(location = 8) in vec4 s_vWeights2;

// --- Outputs ---
out vec3 normal;
out vec3 frag_p;
out vec2 TexCoord;

// Breathing displacement in the mesh's own space (feet at y = 0, torso between
// y ~ 1.7 and 3.0): the rib cage swells along its normals, the shoulders, neck
// and head rise with it, and the arms sway a little.
vec3 breathe(vec3 p, vec3 n)
{
    // Slow cycle, slightly faster inhale than exhale
    float w = 1.5 * breathTime;
    float s = 0.5 + 0.5 * sin(w - 0.55 * sin(w));          // 0..1

    float chest = smoothstep(1.8, 2.3, p.y) * (1.0 - smoothstep(2.9, 3.2, p.y));
    chest *= 1.0 - smoothstep(0.35, 0.60, abs(p.x));        // not the arms
    float upper = smoothstep(2.3, 3.0, p.y);                // shoulders, neck, head

    vec3 d = n * (0.050 * chest * s);
    d.y += 0.050 * upper * s;
    d.z += -0.025 * upper * s;                              // chest pushes the head back
    // Arms hang loosely and sway a little behind the breath
    float arm = smoothstep(0.35, 0.60, abs(p.x)) * smoothstep(0.0, 2.9, p.y);
    d.z += 0.020 * arm * sin(w - 0.8);
    return p + breathAmp * d;
}

// Idle pose of the penguin, done on its mesh (bind pose: a T with flat
// flippers; x sideways, y up; 5.25 tall, body 0.6 half width, flippers from
// x = 0.6 to 3.3 at y = 3.55): the flippers hang down along the body and
// the penguin breathes calmly, slowly and a bit faster in than out. Every
// change depends only on the position, so the faces of the mesh (which do not
// share their vertices) stay together. The normal turns with the flippers.
void idle(inout vec3 p, inout vec3 n)
{
    float w = 1.5 * breathTime;
    float s = 0.5 + 0.5 * sin(w - 0.55 * sin(w));          // 0..1

    // The flippers hang from the shoulder: the farther from the body, the more
    // they turn (the first part bends like a joint), with a little sway
    float side = p.x < 0.0 ? -1.0 : 1.0;
    // (the flippers of the mesh are too long for a penguin: they are shortened
    // to 62 % before they hang)
    float ax = abs(p.x);
    float ox = ax > 0.6 ? 0.6 + (ax - 0.6) * 0.62 : ax;
    float k = smoothstep(0.6, 0.95, ox);
    float chest = smoothstep(1.2, 2.0, p.y) * (1.0 - smoothstep(3.3, 3.9, p.y)) * (1.0 - k);
    float upper = smoothstep(3.0, 4.0, p.y);
    float a = -side * k * (1.47 + 0.04 * sin(w - 0.8));      // 1.47 rad = 84 degrees
    float c = cos(a), sn = sin(a);
    vec3 hinge = vec3(side * 0.6, 3.55, 0.0);
    p.x = side * ox;
    vec3 r = p - hinge;
    p = hinge + vec3(r.x * c - r.y * sn, r.x * sn + r.y * c, r.z);
    n = vec3(n.x * c - n.y * sn, n.x * sn + n.y * c, n.z);

    // The rib cage swells a little (more front to back than sideways: the front
    // is -z) and comes forward, the shoulders and head rise and go back a
    // little, and the upper body sways very slowly on top of it
    // (measured on the mesh, 0.343 m per unit: the chest gets 3 % wider and
    // 6 % deeper, its front moves ~4.5 cm, the head rises ~2 cm)
    p.x *= 1.0 + 0.03 * chest * s;
    p.z = -0.3 + (p.z + 0.3) * (1.0 + 0.06 * chest * s) - 0.10 * chest * s;
    p.y += 0.06 * upper * s;
    p.z += upper * (0.05 * s + 0.04 * sin(0.35 * breathTime));
}

// Wind on a tree, done on the world position of a vertex: the whole tree leans
// with the gusts (more the higher the vertex is: the trunk is stiff at the
// base) and its leaves, far from the trunk, flutter. `local` is the vertex in
// the mesh's own space, `scale` the object's scale. Every tree moves a little
// differently (the phase comes from where it stands). Normals are not changed.
vec3 sway(vec3 worldPos, vec3 local, float scale)
{
    float h = max(local.y, 0.0) * scale;                // metres over the foot
    float r = length(local.xz) * scale;                 // metres from the trunk
    float phase = objposition.x * 0.37 + objposition.z * 0.23;
    float gust = sin(breathTime * 0.8 + phase) + 0.5 * sin(breathTime * 1.9 + phase * 1.7) + 0.35;
    float bend = (h * h) / 196.0;                       // 14 m tall: 1
    vec3 wind = normalize(vec3(1.0, 0.0, 0.35));
    vec3 d = wind * (0.35 * gust * bend);
    float flutter = sin(breathTime * 4.0 + dot(worldPos, vec3(1.3, 1.9, 0.7)) + phase);
    d += vec3(0.4, 1.0, 0.25) * (0.045 * flutter * smoothstep(2.0, 7.0, h) * min(r, 3.0));
    return worldPos + swayAmp * d;
}

void main()
{
    // ---- Skinning ----
    float total = skinned == 0 ? 0.0 : dot(s_vWeights0, vec4(1.0)) + dot(s_vWeights1, vec4(1.0))
                + dot(s_vWeights2, vec4(1.0));
    mat4 BMatrix;
    if (total > 0.0) {
        BMatrix = mat4(0.0);
        for (int k = 0; k < 4; k++) {
            BMatrix += gBones[s_vIDs0[k]] * s_vWeights0[k];
            BMatrix += gBones[s_vIDs1[k]] * s_vWeights1[k];
            BMatrix += gBones[s_vIDs2[k]] * s_vWeights2[k];
        }
    } else if (skinned != 0) {
        BMatrix = meshMat;
    } else {
        BMatrix = mat4(1.0);
    }

    vec3 local = aPos;
    vec3 inNormal = Normal;
    if (skinned == 0 && breathAmp > 0.0)
        local = breathe(aPos, Normal);
    else if (skinned != 0 && idlePose != 0)
        idle(local, inNormal);
    vec4 pos = BMatrix * vec4(local, 1.0);
    vec3 fitted = skinned != 0 ? (pos.xyz - fitCenter) * fitScale : pos.xyz;

    // ---- Object transform (same convention as shader.vert) ----
    vec3 worldPos = vec3(objrotation * vec4(fitted, 1.0)) + objposition;

    if (skinned == 0 && swayAmp > 0.0)
        worldPos = sway(worldPos, fitted, length(objrotation[1].xyz));
    gl_Position = projection * model * view * vec4(worldPos, 1.0);
    frag_p = worldPos;

    // ---- Normals ----
    normal = mat3(objrotation) * mat3(BMatrix) * inNormal;

    TexCoord = aTexCoord;
}
