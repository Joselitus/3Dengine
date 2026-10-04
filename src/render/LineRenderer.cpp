#include "LineRenderer.h"

#include <cmath>

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Shader.h"

using namespace glm;

#define FLOATS_PER_VERTEX 7 // position (3), colour (4)

LineRenderer::LineRenderer() {
  // Creating a Shader leaves it in use: give the engine's program back
  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  shader.reset(new Shader("shaders/lines.vert", "shaders/lines.frag"));
  glUseProgram(previous);

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  const GLsizei stride = FLOATS_PER_VERTEX * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));
  glBindVertexArray(0);
}

LineRenderer::~LineRenderer() {
  if (VBO) {
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
  }
}

void LineRenderer::line(const vec3 &a, const vec3 &b, const vec4 &color) {
  vertices.insert(vertices.end(), {a.x, a.y, a.z, color.r, color.g, color.b,
                                   color.a, b.x, b.y, b.z, color.r, color.g,
                                   color.b, color.a});
}

void LineRenderer::box(const vec3 &min, const vec3 &max, const vec4 &color) {
  box((min + max) * 0.5f, mat3(1.0f), (max - min) * 0.5f, color);
}

void LineRenderer::box(const vec3 &centre, const mat3 &axes, const vec3 &half,
                       const vec4 &color) {
  // Corner i has the sign of half[k] given by bit k of i
  vec3 corners[8];
  for (int i = 0; i < 8; i++) {
    corners[i] = centre;
    for (int k = 0; k < 3; k++)
      corners[i] += axes[k] * (half[k] * ((i >> k) & 1 ? 1.0f : -1.0f));
  }
  // The edges join corners that differ in one bit
  for (int i = 0; i < 8; i++)
    for (int k = 0; k < 3; k++)
      if (!((i >> k) & 1))
        line(corners[i], corners[i | (1 << k)], color);
}

void LineRenderer::circle(const vec3 &centre, const vec3 &normal, float radius,
                          const vec4 &color, int segments) {
  // Two directions across the normal
  vec3 u = cross(normal, std::fabs(normal.y) < 0.9f ? vec3(0, 1, 0)
                                                    : vec3(1, 0, 0));
  u = normalize(u);
  vec3 v = cross(normal, u);
  vec3 previous = centre + u * radius;
  for (int i = 1; i <= segments; i++) {
    float angle = 6.2831853f * i / segments;
    vec3 next = centre + (u * std::cos(angle) + v * std::sin(angle)) * radius;
    line(previous, next, color);
    previous = next;
  }
}

void LineRenderer::arc(const vec3 &centre, const vec3 &u, const vec3 &v,
                       float radius, const vec4 &color, int segments) {
  vec3 previous = centre + u * radius;
  for (int i = 1; i <= segments; i++) {
    float angle = 3.14159265f * i / segments;
    vec3 next = centre + (u * std::cos(angle) + v * std::sin(angle)) * radius;
    line(previous, next, color);
    previous = next;
  }
}

void LineRenderer::draw(Camera &camera) {
  if (vertices.empty())
    return;
  GLint previousProgram, previousVAO;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
  GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST); // seen through everything

  shader->use();
  mat4 viewProjection = camera.getViewProjection();
  shader->setMatrix4("viewProjection", value_ptr(viewProjection));

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  size_t count = vertices.size() / FLOATS_PER_VERTEX;
  if (count > capacity) {
    capacity = count * 2;
    glBufferData(GL_ARRAY_BUFFER, capacity * FLOATS_PER_VERTEX * sizeof(float),
                 nullptr, GL_DYNAMIC_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float),
                  vertices.data());
  glDrawArrays(GL_LINES, 0, (GLsizei)count);
  vertices.clear();

  glBindVertexArray(previousVAO);
  if (depth)
    glEnable(GL_DEPTH_TEST);
  if (!blend)
    glDisable(GL_BLEND);
  glUseProgram(previousProgram);
}
