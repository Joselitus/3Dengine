#version 330 core
// Film grain (FilmGrain): specks of two pixels that change 24 times a second, scattered over the
// picture: a few light ones and a few dark ones (so it shows on a dark night without washing it
// out), the rest of the screen untouched.

uniform float opacity; // 0..1
uniform float time;    // seconds

out vec4 FragColor;

float hash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

void main() {
    vec2 cell = floor(gl_FragCoord.xy / 2.0);
    float frame = floor(time * 24.0);
    float n = hash(cell + vec2(frame * 37.0, frame * 91.0));
    // only the cells far from the middle show: light above it, dark below
    float speck = smoothstep(0.3, 1.0, abs(n - 0.5) * 2.0);
    vec3 shade = n > 0.5 ? vec3(0.75) : vec3(0.0);
    FragColor = vec4(shade, opacity * speck);
}
