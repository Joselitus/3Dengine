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
  // when a bone is animated). A bone it does not name follows its nearest ancestor that it does
  // name, as if rigidly attached to it (the fingers of a hand): only if it has none does it keep
  // its last pose, unless `orphansFollow` names a posed bone: then those
  // bones (the root, helper bones of the rig) follow that one.
  void SetPose(const std::unordered_map<std::string, glm::mat4> &globals,
               const std::string &orphansFollow = "");
  // Tells it where every bone of the file is in the bind pose, even the ones no vertex is weighted
  // to (and so are not in `bones`): the inverses of their offset matrices, by name. SetPose uses
  // them so that a bone follows its nearest ancestor even through such bones.
  void SetBindPoses(const std::unordered_map<std::string, glm::mat4> &offsets);
  // A bone's matrix in the pose the skeleton has now (what SetPose takes), whether or not any
  // vertex is weighted to it (then it is worked out from the nearest ancestor that is). False if
  // the file has no such bone.
  bool PoseGlobal(const std::string &name, glm::mat4 &matrix) const;
  // A bone's matrix in the bind pose (false if the file has no such bone)
  bool BindGlobal(const std::string &name, glm::mat4 &matrix) const;
  // Some importers (Assimp 5.4 with the penguin's FBX) leave the file's unit scale on the mesh's node
  // instead of in the offsets: the offsets then expect vertices 100 times bigger than they are. This
  // matrix (the mesh node's) is applied to the vertices before skinning to put that right: every
  // bone matrix gets it on its right (see Update, SetPose, PoseGlobal). Identity (the default) when
  // the file is consistent.
  void SetSkinCorrection(const glm::mat4 &m) { skinFix = m; }
  // Object-space transform of a node for the last Update().
  glm::mat4 NodeGlobal(const aiNode *node) const;

private:
  glm::mat4 skinFix = glm::mat4(1.0f);
  aiNode *root;
  const aiAnimation *animation;
  std::unordered_map<std::string, const aiNodeAnim *> channels;
  std::unordered_map<const aiNode *, glm::mat4> nodeGlobals;
  std::unordered_map<std::string, glm::mat4> bindGlobals; // each bone's matrix in the bind pose
  std::unordered_map<std::string, std::string> boneParents; // nearest ancestor that is a bone

  void Traverse(const aiNode *node, const glm::mat4 &parent, double ticks);
  glm::mat4 LocalTransform(const aiNode *node, double ticks) const;
};

#endif
