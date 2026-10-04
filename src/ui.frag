#version 330 core
// 2D interface (UIRenderer): flat, alpha-blended colour.

in vec4 color;
out vec4 FragColor;

void main() {
    FragColor = color;
}
