#version 330 core

in vec2 corner;
in vec4 color;
in vec3 worldPos;
out vec4 FragColor;

// The map's light, so that a particle is as lit as what is around it: the
// colour and direction (towards the light) of the sun, as in shader.frag, and
// the spot lights (same uniforms as there: see shader.frag)
uniform vec3 sunColor;
uniform vec3 sunDir;
const int MAX_SPOTS = 8;
uniform int spotCount;
uniform vec3 spotPosition[MAX_SPOTS];
uniform vec3 spotDirection[MAX_SPOTS];
uniform vec3 spotColor[MAX_SPOTS];
uniform vec3 spotParams[MAX_SPOTS];

void main() {
	// a disc with a soft edge
	float d = length(corner);
	if (d >= 1.0)
		discard;
	float edge = 1.0 - smoothstep(0.55, 1.0, d);

	// A cloud has no surface to take a normal from: it gets the ambient light
	// of the world shader plus the sun with a "half Lambert" against the
	// ground's normal (up), so it is bright at noon, orange at dusk and nearly
	// dark at night, like the sand it lies on
	vec3 light = sunColor * (0.3 + 0.7 * (0.5 + 0.5 * sunDir.y));
	for (int i = 0; i < spotCount; i++) {
		vec3 toLight = spotPosition[i] - worldPos;
		float dist = length(toLight);
		float cone = smoothstep(spotParams[i].y, spotParams[i].x,
		                        dot(-toLight / dist, spotDirection[i]));
		float fall = clamp(1.0 - dist / spotParams[i].z, 0.0, 1.0);
		light += 0.7 * cone * fall * fall * spotColor[i];
	}
	FragColor = vec4(color.rgb * min(light, vec3(1.0)), color.a * edge);
}
