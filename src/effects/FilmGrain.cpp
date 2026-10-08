#include "FilmGrain.h"

#include <GL/glew.h>

#include "Shader.h"

FilmGrain::FilmGrain() {
  // Creating a Shader leaves it in use: give the engine's program back
  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  shader.reset(new Shader("shaders/grain.vert", "shaders/grain.frag"));
  glUseProgram(previous);
  glGenVertexArrays(1, &VAO); // (no attributes: the vertex shader makes the triangle)
}

FilmGrain::~FilmGrain() {}

void FilmGrain::draw(float amount, float time) {
  if (amount <= 0.0f)
    return;
  GLint previousProgram, previousVAO;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
  GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);

  shader->use();
  shader->setFloat("opacity", MAX_OPACITY * (amount < 1.0f ? amount : 1.0f));
  shader->setFloat("time", time);
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glBindVertexArray(previousVAO);
  if (depth)
    glEnable(GL_DEPTH_TEST);
  if (!blend)
    glDisable(GL_BLEND);
  glUseProgram(previousProgram);
}
