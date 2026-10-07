#ifndef MESH
#define MESH
#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "Shader.h"

struct Vertex {
  glm::vec3 Position;
  glm::vec3 Normal;
  glm::vec2 TexCoords;
};

struct Texture {
  unsigned int id;
  std::string type;
  std::string path;
};

// Static geometry uploaded to the GPU (VAO + VBO + EBO). Attributes:
// location 0 position, 1 normal, 2 uv. Draw() binds the textures as
// texture_diffuseN / texture_specularN uniforms (N from 1).
class Mesh {
private:
  // mesh data
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;
  // Flat material colour, used when the material has no diffuse texture
  bool hasColor = false;
  std::string materialName; // as named in the model file (e.g. the .mtl)
  glm::vec3 color = glm::vec3(1.0f);
  float opacity = 1.0f; // < 1: translucent (see Model::Draw)

  //  render data
  unsigned int VAO, VBO, EBO;
  void setupMesh();

public:
  Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices,
       std::vector<Texture> textures);
  virtual ~Mesh() = default;
  const std::vector<Vertex> &getVertices() const { return vertices; }
  const std::vector<unsigned int> &getIndices() const { return indices; }
  const std::vector<Texture> &getTextures() const { return textures; }
  void setMaterialName(const std::string &name) { materialName = name; }
  const std::string &getMaterialName() const { return materialName; }
  void setColor(const glm::vec3 &c) { color = c; hasColor = true; }
  void setOpacity(float o) { opacity = o; }
  float getOpacity() const { return opacity; }
  bool isTransparent() const { return opacity < 1.0f; }
  // A translucent mesh is blended over what is already drawn (and does not
  // write depth), so draw those after everything opaque
  void Draw(Shader *shader);
};

#endif
