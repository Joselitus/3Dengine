#ifndef ANIMATED_MESH
#define ANIMATED_MESH
#define GLM_ENABLE_EXPERIMENTAL

#include "Mesh.h"

// The rig has vertices with up to 9 bone influences; truncating to 4 distorts
// them (e.g. the hood strings get dragged by the arm), so allow 12 = 3 vec4s.
#define NUM_BONES_PER_VEREX 12

struct AnimatedVertex {
  glm::vec3 Position;
  glm::vec3 Normal;
  glm::vec2 TexCoords;
  int BoneIDs[NUM_BONES_PER_VEREX] = {};
  float Weights[NUM_BONES_PER_VEREX] = {};
};

// Like Mesh, plus up to NUM_BONES_PER_VEREX (bone id, weight) pairs per
// vertex, uploaded as three ivec4/vec4 attribute pairs (locations 3 to 8).
// Bone ids index the model-wide list kept by the Skeleton.
class AnimatedMesh { // TODO Very carefully refactor this to extend Model
private:
  std::vector<AnimatedVertex> anivertices;

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  //  render data
  unsigned int VAO, VBO, EBO;
  void setupMesh();

public:
  const std::vector<AnimatedVertex> &getVertices() const { return anivertices; }
  AnimatedMesh(std::vector<AnimatedVertex> anivertices,
               std::vector<unsigned int> indices,
               std::vector<Texture> textures);

  virtual ~AnimatedMesh() = default;
  void Draw(Shader *shader);
};

#endif
