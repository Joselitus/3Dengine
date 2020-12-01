uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec3 objposition;
uniform mat4 pengrotation;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

out vec3 normal;
out vec3 frag_p;
out vec2 TexCoord;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in ivec4 s_vIDs;
layout (location = 4) in vec4 s_vWeights;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

in vec4 s_vWeights;
in ivec4 s_vIDs;

void main ()
{
     mat4 BMatrix = gBones[s_vIDs[0]] * s_vWeights[0];
          BMatrix += gBones[s_vIDs[1]] * s_vWeights[1];
          BMatrix += gBones[s_vIDs[2]] * s_vWeights[2];
          BMatrix += gBones[s_vIDs[3]] * s_vWeights[3];

     vec3 trasPos = aPos + objposition;
 	 gl_Position = projection*model*view*BMatrix*vec4(trasPos, 1.0);
     frag_p = VPMatrix * MMatrix * BMatrix * (vec4(s_vPosition,1.0));
     normal = vec3(pengrotation*vec4(Normal, 1.0));
	 frag_p = trasPos;
	 TexCoord = aTexCoord;
 
}