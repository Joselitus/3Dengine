#ifndef ANIMATED_MESH
#define ANIMATED_MESH

#include <vector>

#include "Mesh.h"
class Skeleton;

#define NUM_BONES_PER_VEREX 4

struct AnimatedVertex {
	glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::ivec4 BoneIDs;
    glm::vec4 Weights;

};

class AnimatedMesh: public Mesh {
private:
	std::vector<AnimatedVertex> anivertices;

public:
	Skeleton * skeleton;
	void updateSkeleton();
	AnimatedMesh(std::vector<AnimatedVertex> anivertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
	void setupMesh();
};

#include "Skeleton.h"

#endif