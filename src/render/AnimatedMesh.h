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

  // Flat colours for a material without a texture, and a glow (emissive) colour
  bool hasColor = false, hasEmissive = false;
  bool glowing = true;
  glm::vec3 color = glm::vec3(1.0f), emissive = glm::vec3(0.0f);

  //  render data
  unsigned int VAO, VBO, EBO;
  void setupMesh();

public:
  const std::vector<AnimatedVertex> &getVertices() const { return anivertices; }
  AnimatedMesh(std::vector<AnimatedVertex> anivertices,
               std::vector<unsigned int> indices,
               std::vector<Texture> textures);

  virtual ~AnimatedMesh() = default;
  // The material has no texture: draw it with this colour; with `glowing` it is
  // emissive (drawn flat, unaffected by the light: it glows)
  void setColor(const glm::vec3 &c) { color = c; hasColor = true; }
  void setEmissive(const glm::vec3 &c) { emissive = c; hasEmissive = true; }
  // An emissive mesh stops glowing (glowing = false): it is drawn lit, in its colour much darker
  void setGlowing(bool on) { glowing = on; }
  void Draw(Shader *shader);
};

#endif
