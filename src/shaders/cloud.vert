#version 330 core
// Clouds (CloudRenderer): one triangle that covers the whole screen, made from the vertex index;
// the fragment shader finds where the view ray meets the cloud's box.

void main() {
    vec2 corner = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    gl_Position = vec4(corner, 0.0, 1.0);
}
