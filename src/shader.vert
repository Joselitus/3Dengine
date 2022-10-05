#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec2 aTexCoord;

//borrar cuando no este salva
uniform vec3 objposition;
uniform mat4 pengrotation;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 normal;
out vec3 frag_p;
out vec2 TexCoord;

void main() {
	vec3 rotaPos = vec3(pengrotation*vec4(aPos, 1.0));
	vec3 trasPos = rotaPos + objposition;
	gl_Position = projection*model*view*vec4(trasPos, 1.0);
	normal = vec3(pengrotation*vec4(Normal, 1.0));
	frag_p = trasPos;
	TexCoord = aTexCoord;
}