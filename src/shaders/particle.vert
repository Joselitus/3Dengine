#version 330 core

// One corner of a particle's disc: a quad that always faces the camera,
// centred on the particle (the world position) and as big as its size (radius)
layout(location = 0) in vec3 aCenter;
layout(location = 1) in vec2 aCorner; // -1..1 across the quad
layout(location = 2) in float aSize;
layout(location = 3) in vec4 aColor;

uniform mat4 viewProjection; // projection * model * view, as the world shader
uniform vec3 camRight;
uniform vec3 camUp;

out vec2 corner;
out vec4 color;

void main() {
	vec3 world = aCenter + (camRight * aCorner.x + camUp * aCorner.y) * aSize;
	gl_Position = viewProjection * vec4(world, 1.0);
	corner = aCorner;
	color = aColor;
}
