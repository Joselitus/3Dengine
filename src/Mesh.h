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

class Mesh {
private:
  // mesh data
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;
  // Flat material colour, used when the material has no diffuse texture
  bool hasColor = false;
  glm::vec3 color = glm::vec3(1.0f);

  //  render data
  unsigned int VAO, VBO, EBO;
  void setupMesh();

public:
  Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices,
       std::vector<Texture> textures);
  Mesh(std::vector<unsigned int> indices, std::vector<Texture> textures);
  virtual ~Mesh() = default;
  const std::vector<Vertex> &getVertices() const { return vertices; }
  const std::vector<unsigned int> &getIndices() const { return indices; }
  void setColor(const glm::vec3 &c) { color = c; hasColor = true; }
  void Draw(Shader *shader);
};

#endif
