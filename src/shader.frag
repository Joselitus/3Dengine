#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 normal;
in vec3 frag_p;

uniform vec3 lightColor;
uniform vec3 lightPosition;
uniform vec3 viewPosition;

// 0 = lit, 1 = sky dome (unlit, stars twinkle), 2 = emissive (flat colour)
uniform int unlit;
uniform float time;
uniform vec3 moonDir;
// Distant geometry fades into this colour (matches the sky at the horizon)
uniform vec3 fogColor;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform sampler2D texture_diffuse3;
uniform sampler2D texture_specular1;
uniform sampler2D texture_specular2;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
	if (unlit == 2) {
		FragColor = vec4(texture(texture_diffuse1, TexCoord).rgb, 1.0);
		return;
	}
	if (unlit == 1) {
		vec3 c = texture(texture_diffuse1, TexCoord).rgb;
		vec3 dir = normalize(frag_p - viewPosition);
		// Twinkle the stars (bright texels), but not the moon and its glow
		float star = smoothstep(0.35, 0.9, max(c.r, max(c.g, c.b)));
		float away = 1.0 - smoothstep(0.985, 0.995, dot(dir, moonDir));
		float phase = hash(floor(TexCoord * vec2(1024.0, 512.0))) * 6.2831;
		float tw = 1.0 - 0.45 * star * away * (0.5 + 0.5 * sin(time * 2.5 + phase));
		FragColor = vec4(c * tw, 1.0);
		return;
	}
	float ambientStrength = 0.3;
	float specularStrength = 0.15;

	vec3 lightdir = normalize(lightPosition - frag_p);
	vec3 norm = normalize(normal);
	vec3 viewDir = normalize(viewPosition - frag_p);
	vec3 reflectDir = reflect(-lightdir, norm);  	
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128);
	vec3 specular = specularStrength * spec * lightColor;
	float diff = max(dot(norm, lightdir), 0.0);
	vec3 diffuse = diff * lightColor;
	vec3 ambient = ambientStrength*lightColor;
	vec3 lit = texture(texture_diffuse1, TexCoord).rgb*(ambient+diffuse+specular);
	float fog = smoothstep(30.0, 70.0, distance(frag_p, viewPosition));
	FragColor = vec4(mix(lit, fogColor, fog), 1.0);
}