#version 330 core

in vec2 corner;
in vec4 color;
out vec4 FragColor;

void main() {
	// a disc with a soft edge
	float d = length(corner);
	if (d >= 1.0)
		discard;
	float edge = 1.0 - smoothstep(0.55, 1.0, d);
	FragColor = vec4(color.rgb, color.a * edge);
}
