#include "Mesh.h"
#include "RenderStats.h"
using namespace std;
using namespace glm;

Mesh::Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture> textures) {
	this->vertices = vertices;
	this->indices = indices;
	this->textures = textures;

	this->setupMesh();
}

void Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
  
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);  

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                 &indices[0], GL_STATIC_DRAW);

    // vertex positions
    glEnableVertexAttribArray(0);	
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    // vertex normals
    glEnableVertexAttribArray(1);	
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    // vertex texture coords
    glEnableVertexAttribArray(2);	
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glBindVertexArray(0);
} 

void Mesh::Draw(Shader * shader) {
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    for(unsigned int i = 0; i < textures.size(); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i); // activate proper texture unit before binding
        // retrieve texture number (the N in diffuse_textureN)
        string number;
        string name = textures[i].type;
        if(name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if(name == "texture_specular")
            number = std::to_string(specularNr++);

        shader->setInt((name + number).c_str(), i);
        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }
    glActiveTexture(GL_TEXTURE0);
    shader->setInt("useColor", hasColor ? 1 : 0);
    shader->setFloat("alpha", opacity);
    if (hasColor)
        shader->setVector3("diffuseColor", color.x, color.y, color.z);

    // draw mesh
    bool blended = isTransparent();
    GLboolean wasBlend = GL_FALSE;
    if (blended) {
        wasBlend = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // it does not hide what is behind it
    }
    glBindVertexArray(VAO);
    RenderStats::draws()++;
    RenderStats::triangles() += indices.size() / 3;
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    if (blended) {
        glDepthMask(GL_TRUE);
        if (!wasBlend)
            glDisable(GL_BLEND);
        shader->setFloat("alpha", 1.0f);
    }
}
Mesh Mesh::deformed(const std::function<glm::vec3(const glm::vec3 &)> &move) const {
  Mesh result = *this; // (the material is copied; setupMesh gives it buffers of its own)
  std::vector<glm::vec3> sums(vertices.size(), glm::vec3(0.0f));
  std::vector<float> moved(vertices.size(), 0.0f);
  for (size_t i = 0; i < vertices.size(); i++) {
    result.vertices[i].Position = move(vertices[i].Position);
    moved[i] = glm::length(result.vertices[i].Position - vertices[i].Position);
  }
  // area-weighted normals of the new triangles, added at their corners
  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    unsigned a = indices[i], b = indices[i + 1], c = indices[i + 2];
    glm::vec3 n = glm::cross(result.vertices[b].Position - result.vertices[a].Position,
                             result.vertices[c].Position - result.vertices[a].Position);
    // (keep the winding's side: the old normal says which way is out)
    glm::vec3 old = vertices[a].Normal + vertices[b].Normal + vertices[c].Normal;
    if (glm::dot(n, old) < 0.0f)
      n = -n;
    sums[a] += n;
    sums[b] += n;
    sums[c] += n;
  }
  for (size_t i = 0; i < vertices.size(); i++)
    if (moved[i] > 1e-4f && glm::length(sums[i]) > 1e-8f)
      result.vertices[i].Normal = glm::normalize(
          glm::mix(vertices[i].Normal, glm::normalize(sums[i]), glm::min(moved[i] * 10.0f, 1.0f)));
  result.setupMesh();
  return result;
}
