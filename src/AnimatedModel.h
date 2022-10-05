#ifndef ANIMATED_MODEL
#define ANIMATED_MODEL

#include "Model.h"
#include "Skeleton.h"

class AnimatedModel: public Model {
private:
	std::vector<aiNodeAnim*> nodes_anim;
	aiNode * rootNode;
	glm::mat4 globalInverseTransform;
	std::vector<Bone> bones;
	std::vector<AnimatedMesh> meshes;

	void loadModel(std::string path);
	void recursiveNodeProcess(aiNode* node);
	void AnimNodeProcess();
	Bone* FindBone(std::string name);
	aiNode* FindAiNode(std::string name);
	aiNodeAnim* FindAiNodeAnim(std::string name);
	int FindBoneIDByName(std::string name);
	Mesh processMesh(aiMesh * mesh, const aiScene * scene);

public:
	AnimatedModel(const char * path);
};

#endif