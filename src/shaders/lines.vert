#version 330 core

// A vertex of a debug line (LineRenderer), in the world
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 viewProjection; // projection * model * view, as the world shader

out vec4 color;

void main() {
	gl_Position = viewProjection * vec4(aPosition, 1.0);
	color = aColor;
}
