#version 330 core
// 2D interface (UIRenderer): positions in window pixels, y down.

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

uniform vec2 screen; // window size in pixels

out vec4 color;

void main() {
    vec2 ndc = aPos / screen * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    color = aColor;
}
