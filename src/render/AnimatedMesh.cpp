#include "AnimatedMesh.h"
using namespace std;

AnimatedMesh::AnimatedMesh(std::vector<AnimatedVertex> anivertices,
                           std::vector<unsigned int> indices,
                           std::vector<Texture> textures) {
  this->anivertices = anivertices;
  this->indices = indices;
  this->textures = textures;
  this->setupMesh();
}

void AnimatedMesh::setupMesh() {
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  glBufferData(GL_ARRAY_BUFFER, anivertices.size() * sizeof(AnimatedVertex),
               &anivertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               &indices[0], GL_STATIC_DRAW);

  // vertex positions
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
                        (void *)0);
  // vertex normals
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
                        (void *)offsetof(AnimatedVertex, Normal));
  // vertex texture coords
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
                        (void *)offsetof(AnimatedVertex, TexCoords));
  // vertex bone indices and weights, 4 per attribute pair
  for (int g = 0; g < NUM_BONES_PER_VEREX / 4; g++) {
    glEnableVertexAttribArray(3 + 2 * g);
    glVertexAttribIPointer(
        3 + 2 * g, 4, GL_INT, sizeof(AnimatedVertex),
        (void *)(offsetof(AnimatedVertex, BoneIDs) + g * 4 * sizeof(int)));
    glEnableVertexAttribArray(4 + 2 * g);
    glVertexAttribPointer(
        4 + 2 * g, 4, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
        (void *)(offsetof(AnimatedVertex, Weights) + g * 4 * sizeof(float)));
  }

  glBindVertexArray(0);
}

void AnimatedMesh::Draw(Shader *shader) {
  unsigned int diffuseNr = 1;
  unsigned int specularNr = 1;
  for (unsigned int i = 0; i < textures.size(); i++) {
    glActiveTexture(GL_TEXTURE0 +
                    i); // activate proper texture unit before binding
    // retrieve texture number (the N in diffuse_textureN)
    string number;
    string name = textures[i].type;
    if (name == "texture_diffuse")
      number = std::to_string(diffuseNr++);
    else if (name == "texture_specular")
      number = std::to_string(specularNr++);

    shader->setInt((name + number).c_str(), i);
    glBindTexture(GL_TEXTURE_2D, textures[i].id);
  }
  glActiveTexture(GL_TEXTURE0);
  // A flat colour when there is no texture (and a glow when it is emissive)
  bool flat = textures.empty() && (hasColor || hasEmissive);
  shader->setInt("useColor", flat ? 1 : 0);
  if (flat) {
    glm::vec3 shown = hasEmissive ? (glowing ? emissive : color * 0.12f) : color;
    shader->setVector3("diffuseColor", shown.x, shown.y, shown.z);
  }
  if (hasEmissive && glowing)
    shader->setInt("unlit", 2);
  shader->setFloat("alpha", 1.0f);

  // draw mesh
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  if (hasEmissive && glowing)
    shader->setInt("unlit", 0);
}
