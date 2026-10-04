#include "ParticleRenderer.h"

#include <algorithm>

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Shader.h"

using namespace glm;

// position (3), corner (2), size (1), colour (4)
#define FLOATS_PER_VERTEX 10

ParticleRenderer::ParticleRenderer() {
  // Creating a Shader leaves it in use: give the engine's program back
  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  shader.reset(new Shader("shaders/particle.vert", "shaders/particle.frag"));
  glUseProgram(previous);

  // The quads' indices never change: 0 1 2, 0 2 3 for each
  std::vector<unsigned int> indices;
  for (int i = 0; i < MAX_PARTICLES; i++)
    for (unsigned int k : {0u, 1u, 2u, 0u, 2u, 3u})
      indices.push_back(4 * i + k);

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER,
               MAX_PARTICLES * 4 * FLOATS_PER_VERTEX * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  const GLsizei stride = FLOATS_PER_VERTEX * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                        (void *)(5 * sizeof(float)));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                        (void *)(6 * sizeof(float)));
  glBindVertexArray(0);
}

ParticleRenderer::~ParticleRenderer() {
  if (VBO) {
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);
  }
}

void ParticleRenderer::draw(
    const std::vector<std::shared_ptr<ParticleEmitter>> &emitters,
    Camera &camera) {
  // Every particle of every emitter, with how far it is from the camera
  struct Item {
    const Particle *particle;
    const ParticleEmitter::Settings *settings;
    float distance2;
  };
  std::vector<Item> items;
  vec3 eye = camera.getPosition();
  for (const auto &emitter : emitters)
    for (const Particle &p : emitter->getParticles()) {
      vec3 d = p.position - eye;
      items.push_back({&p, &emitter->getSettings(), dot(d, d)});
    }
  if (items.empty())
    return;
  // farthest first (the nearest ones are the ones kept if there are too many)
  std::sort(items.begin(), items.end(),
            [](const Item &a, const Item &b) { return a.distance2 > b.distance2; });
  if ((int)items.size() > MAX_PARTICLES)
    items.erase(items.begin(), items.end() - MAX_PARTICLES);

  vertices.clear();
  const float corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  for (const Item &item : items) {
    const Particle &p = *item.particle;
    const ParticleEmitter::Settings &s = *item.settings;
    float t = ParticleEmitter::lifeFraction(p);
    float size = s.sizeStart + (s.sizeEnd - s.sizeStart) * t;
    float alpha = s.alpha * (1.0f - t) * (1.0f - t); // fades out, quickly at the end
    for (const auto &c : corners)
      vertices.insert(vertices.end(),
                      {p.position.x, p.position.y, p.position.z, c[0], c[1],
                       size, s.color.r, s.color.g, s.color.b, alpha});
  }

  // Draw state: blended, hidden by the world but not hiding it
  GLint previousProgram, previousVAO;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
  GLboolean blend = glIsEnabled(GL_BLEND), cull = glIsEnabled(GL_CULL_FACE);
  GLboolean depthMask;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);

  shader->use();
  mat4 viewProjection = camera.getViewProjection();
  shader->setMatrix4("viewProjection", value_ptr(viewProjection));
  vec3 right = camera.getRight(), up = camera.getUp();
  shader->setVector3("camRight", right.x, right.y, right.z);
  shader->setVector3("camUp", up.x, up.y, up.z);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float),
                  vertices.data());
  glDrawElements(GL_TRIANGLES, (GLsizei)(items.size() * 6), GL_UNSIGNED_INT, 0);

  glBindVertexArray(previousVAO);
  glDepthMask(depthMask);
  if (cull)
    glEnable(GL_CULL_FACE);
  if (!blend)
    glDisable(GL_BLEND);
  glUseProgram(previousProgram);
}
