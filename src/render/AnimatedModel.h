#ifndef ANIMATED_MODEL
#define ANIMATED_MODEL

#include "AnimatedMesh.h"
#include "Skeleton.h"

// TODO Very carefully refactor this to extend Model

// Skinned model (FBX) played with GPU skinning. Loading builds the meshes,
// the global bone list and the Skeleton for one animation of the file (the
// first by default), then computeFit() samples that animation to find a centre and a scale that
// make the model ~1.8 units tall around the origin (whatever the units of the
// file). Update(seconds) advances the animation; Draw() uploads gBones,
// fitCenter/fitScale and sets skinned = 1.
class AnimatedModel {
public:
  Assimp::Importer importer; // owns the scene, must outlive every aiNode*
  const aiScene *scene;
  Skeleton skeleton;
  std::vector<AnimatedMesh> meshes;
  std::vector<BoneInfo> pendingBones; // filled while meshes are processed
  std::vector<aiNode *> meshNodes; // node each mesh hangs from

  // model data
  std::vector<Texture> textures_loaded;
  std::string directory;

  // Uniform scale/offset that fits the bind pose in a ~2 unit tall box.
  glm::vec3 fitCenter;
  float fitScale;
  bool feetAtOrigin;
  bool idle = false; // see setIdle()
  glm::mat4 idleMat; // the skin matrix of every bone in the bind pose
  unsigned int animationIndex; // which aiAnimation is played (clamped to the file)

  // feetAtOrigin: computeFit puts the lowest point (the feet) at the
  // object's position instead of the centre, for characters that stand on
  // the floor (e.g. Npc)
  AnimatedModel(const char *path, bool feetAtOrigin = false,
                unsigned int animationIndex = 0);
  AnimatedModel(const AnimatedModel &) = delete;
  AnimatedModel &operator=(const AnimatedModel &) = delete;

  void loadModel(std::string path);
  void processNode(aiNode *node, const aiScene *scene);
  AnimatedMesh processAnimatedMesh(aiMesh *mesh, const aiScene *scene);
  std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type,
                                            std::string typeName);

  // Shows the model not playing its animation but standing still in an idle
  // pose: the bind pose with flippers down and breathing, done by the vertex
  // shader (idlePose, see idle() there: made for the penguin). The size is
  // kept (it was fitted to the animation) but it is re-centred and, with
  // feetAtOrigin, put on its feet for this pose.
  void setIdle(bool idle);
  bool isIdle() const { return idle; }

  // Keep the model's own size and place instead of fitting it to 1.8 units around
  // the origin: for a model made in metres with its feet on y = 0 (the creature)
  void useRealSize();

  void Update(double seconds);
  void Draw(Shader *shader);

private:
  const aiAnimation *chosenAnimation() const;
  void computeFit();
};

#endif
