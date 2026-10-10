#version 330 core
// Clouds (CloudRenderer). The fragment is one pixel of the screen: the view ray through it crosses
// the box from tNear to tFar (tFar cut at the scene's depth, if something solid is in the way). SAMPLES equidistant points of that stretch are
// looked up in the 3D Worley texture (the place inside the box is the texture coordinate) and
// averaged; exp(-average * thickness) is the light of the background that gets through.

const int SAMPLES = 4;

uniform sampler2D depthTexture;
uniform sampler3D noiseTexture;
uniform mat4 inverseViewProjection;
uniform vec3 cameraPosition;
uniform vec3 boxMin;
uniform vec3 boxMax;
uniform vec3 lightColor;
uniform float sunHeight; // the sun's elevation: 1 overhead, 0 on the horizon, < 0 set
uniform vec2 screenSize;
uniform float time;

uniform vec3 octaveWeights;  // how much each octave (R, G, B of the texture) counts
uniform vec3 textureOffset;  // where the box looks into the texture
uniform vec3 drift;          // that, per second
uniform vec3 tiling;         // times the texture repeats across the box, per axis
uniform float threshold;     // the noise below this is a gap between blobs
uniform float absorption;    // optical depth of one metre of density 1
uniform float edgeFade;      // share of the box (from each face) that fades to nothing

out vec4 FragColor;

vec3 worldAt(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 w = inverseViewProjection * clip;
    return w.xyz / w.w;
}

float sampleDensity(vec3 p) {
    vec3 local = (p - boxMin) / (boxMax - boxMin); // 0..1 inside the box
    vec3 tex = local * tiling + textureOffset + drift * time; // (the texture tiles)
    vec3 n = texture(noiseTexture, tex).rgb;
    float shape = dot(n, octaveWeights);
    float density = max(shape - threshold, 0.0) / max(1.0 - threshold, 0.001);
    vec3 toEdge = min(local, 1.0 - local) / max(edgeFade, 0.001);
    float fade = clamp(min(toEdge.x, min(toEdge.y, toEdge.z)), 0.0, 1.0);
    return density * smoothstep(0.0, 1.0, fade);
}

// The colour of the clouds for the sun's height: white by day, then warmer as it gets low, orange
// at the horizon, pink just after it sets, purple, and the dark blue of night; and how bright
vec3 cloudColor() {
    float h = sunHeight;
    vec3 c = vec3(1.0);
    c = mix(c, vec3(1.00, 0.95, 0.88), smoothstep(0.45, 0.22, h));  // afternoon
    c = mix(c, vec3(1.00, 0.72, 0.50), smoothstep(0.22, 0.10, h));  // orange
    c = mix(c, vec3(1.00, 0.52, 0.62), smoothstep(0.10, 0.00, h));  // pink
    c = mix(c, vec3(0.95, 0.45, 0.72), smoothstep(0.00, -0.06, h)); // magenta pink
    c = mix(c, vec3(0.58, 0.36, 0.72), smoothstep(-0.06, -0.14, h)); // purple
    c = mix(c, vec3(0.20, 0.20, 0.42), smoothstep(-0.14, -0.30, h)); // night blue
    float brightness = mix(0.05, 1.0, smoothstep(-0.32, 0.05, h));
    return c * brightness;
}

void main() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    vec3 target = worldAt(uv, 1.0); // a point of the ray, far away
    vec3 dir = normalize(target - cameraPosition);

    // The ray against the box (slab method)
    vec3 inv = 1.0 / dir;
    vec3 t0 = (boxMin - cameraPosition) * inv;
    vec3 t1 = (boxMax - cameraPosition) * inv;
    vec3 tMin = min(t0, t1), tMax = max(t0, t1);
    float tNear = max(max(tMin.x, max(tMin.y, tMin.z)), 0.0);
    float tFar = min(tMax.x, min(tMax.y, tMax.z));

    // The scene: where the ray hits something already drawn (depth 1 = nothing)
    float depth = texture(depthTexture, uv).r;
    if (depth < 1.0)
        tFar = min(tFar, length(worldAt(uv, depth) - cameraPosition));
    if (tFar <= tNear)
        discard;

    // SAMPLES points at the middle of equal parts of the stretch, averaged
    float length_ = tFar - tNear;
    float sum = 0.0;
    for (int i = 0; i < SAMPLES; i++)
        sum += sampleDensity(cameraPosition + dir * (tNear + length_ * (float(i) + 0.5) / float(SAMPLES)));
    float average = sum / float(SAMPLES);

    float transmittance = exp(-average * length_ * absorption);
    // What is left of the light is the cloud itself, coloured by the sun
    FragColor = vec4(cloudColor(), 1.0 - transmittance);
}
