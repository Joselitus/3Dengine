#ifndef SKELETON
#define SKELETON
#define GLM_ENABLE_EXPERIMENTAL

#include "myopengl.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define MAX_BONES 100

// One entry per (mesh, bone) pair. The index in Skeleton::bones is the ID
// stored in the vertices and used to index gBones in the shader.
struct BoneInfo {
  std::string name;
  aiNode *node;
  glm::mat4 offset;
};

// Evaluates one aiAnimation over the node tree. Update(seconds) interpolates
// the keys (looping), computes every node's global transform and fills
// boneMats[i] = global(bone i) * offset(bone i), ready for gBones. Nodes that
// are not animated keep their bind transform.
class Skeleton {
public:
  std::vector<BoneInfo> bones;
  glm::mat4 globalInverseTransform;
  std::vector<glm::mat4> boneMats; // always MAX_BONES entries

  Skeleton();
  void Init(aiNode *root, const aiAnimation *animation,
            std::vector<BoneInfo> in_bones);
  // Evaluates the animation at `seconds` (looping) and fills boneMats.
  void Update(double seconds);
  // Poses the skeleton from outside (a ragdoll): `globals` has, for the bones it names, their
  // object-space matrix (rotation + the position of the bone's joint, like NodeGlobal gives
  // when a bone is animated). The bones it does not name keep their last pose.
  void SetPose(const std::unordered_map<std::string, glm::mat4> &globals);
  // Object-space transform of a node for the last Update().
  glm::mat4 NodeGlobal(const aiNode *node) const;

private:
  aiNode *root;
  const aiAnimation *animation;
  std::unordered_map<std::string, const aiNodeAnim *> channels;
  std::unordered_map<const aiNode *, glm::mat4> nodeGlobals;

  void Traverse(const aiNode *node, const glm::mat4 &parent, double ticks);
  glm::mat4 LocalTransform(const aiNode *node, double ticks) const;
};

#endif
