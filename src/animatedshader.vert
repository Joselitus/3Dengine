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
    if (skinned == 0 && breathAmp > 0.0)
        local = breathe(aPos, Normal);
    vec4 pos = BMatrix * vec4(local, 1.0);
    vec3 fitted = skinned != 0 ? (pos.xyz - fitCenter) * fitScale : pos.xyz;

    // ---- Object transform (same convention as shader.vert) ----
    vec3 worldPos = vec3(objrotation * vec4(fitted, 1.0)) + objposition;

    gl_Position = projection * model * view * vec4(worldPos, 1.0);
    frag_p = worldPos;

    // ---- Normals ----
    normal = mat3(objrotation) * mat3(BMatrix) * Normal;

    TexCoord = aTexCoord;
}
