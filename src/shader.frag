#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 normal;
in vec3 frag_p;

uniform vec3 lightColor;
uniform vec3 lightPosition;
uniform vec3 viewPosition;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform sampler2D texture_diffuse3;
uniform sampler2D texture_specular1;
uniform sampler2D texture_specular2;

void main() {
	float ambientStrength = 0.3;
	float specularStrength = 0.5;

	vec3 lightdir = normalize(lightPosition - frag_p);
	vec3 norm = normalize(normal);
	vec3 viewDir = normalize(viewPosition - frag_p);
	vec3 reflectDir = reflect(-lightdir, norm);  	
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128);
	vec3 specular = specularStrength * spec * lightColor;
	float diff = max(dot(norm, lightdir), 0.0);
	vec3 diffuse = diff * lightColor;
	vec3 ambient = ambientStrength*lightColor;
	FragColor = texture(texture_diffuse1, TexCoord)*vec4(ambient+diffuse+specular, 1.0);
}