#ifndef ANIMATED_MODEL
#define ANIMATED_MODEL

#include "AnimatedMesh.h"
#include "Skeleton.h"

// TODO Very carefully refactor this to extend Model

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

  AnimatedModel(const char *path);
  AnimatedModel(const AnimatedModel &) = delete;
  AnimatedModel &operator=(const AnimatedModel &) = delete;

  void loadModel(std::string path);
  void processNode(aiNode *node, const aiScene *scene);
  AnimatedMesh processAnimatedMesh(aiMesh *mesh, const aiScene *scene);
  std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type,
                                            std::string typeName);

  void Update(double seconds);
  void Draw(Shader *shader);

private:
  void computeFit();
};

#endif
