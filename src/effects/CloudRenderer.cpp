#include "CloudRenderer.h"

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>

#include "Shader.h"

using namespace glm;

namespace {
// Cheap integer hash -> [0, 1)
float hash3(int x, int y, int z, int salt) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)z * 2147483647u +
               (uint32_t)salt * 1274126177u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return (h & 0xFFFFFFu) / 16777216.0f;
}

int wrap(int a, int n) { return ((a % n) + n) % n; }

// Worley noise, F1 (distance to the nearest of one random point per cell), tileable with
// `cells` cells per side; p in [0, 1)
float worley(const vec3 &p, int cells, int salt) {
  vec3 q = p * (float)cells;
  ivec3 base = ivec3(floor(q));
  float best = 4.0f;
  for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        ivec3 c = base + ivec3(dx, dy, dz);
        ivec3 w(wrap(c.x, cells), wrap(c.y, cells), wrap(c.z, cells)); // the point repeats
        vec3 point = vec3(c) + vec3(hash3(w.x, w.y, w.z, salt), hash3(w.x, w.y, w.z, salt + 1),
                                    hash3(w.x, w.y, w.z, salt + 2));
        best = std::min(best, length(q - point));
      }
  return std::min(best, 1.0f);
}
} // namespace

CloudRenderer::CloudRenderer() {
  // Creating a Shader leaves it in use: give the engine's program back
  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  shader.reset(new Shader("shaders/cloud.vert", "shaders/cloud.frag"));
  glUseProgram(previous);

  glGenVertexArrays(1, &VAO); // (no attributes: the vertex shader makes the triangle)
  glGenTextures(1, &noise);
  regenerate();
  glGenTextures(1, &depth);
}

CloudRenderer::~CloudRenderer() {}

// R, G, B: the same noise at 4, 8 and 16 cells, inverted (1 = the heart of a blob) so the shader
// can mix octaves
void CloudRenderer::regenerate() {
  const int N = clamp(settings.resolution, 8, 128);
  std::vector<uint8_t> data((size_t)N * N * N * 3);
  for (int z = 0; z < N; z++)
    for (int y = 0; y < N; y++)
      for (int x = 0; x < N; x++) {
        vec3 p = (vec3(x, y, z) + 0.5f) / (float)N;
        for (int c = 0; c < 3; c++) {
          float v = 1.0f - worley(p, std::max(settings.cells[c], 1), 17 * c + 1000 * settings.seed);
          data[(((size_t)z * N + y) * N + x) * 3 + c] = (uint8_t)(clamp(v, 0.0f, 1.0f) * 255.0f);
        }
      }
  GLint previousTex3D;
  glGetIntegerv(GL_TEXTURE_BINDING_3D, &previousTex3D);
  glBindTexture(GL_TEXTURE_3D, noise);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, N, N, N, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glBindTexture(GL_TEXTURE_3D, previousTex3D);
}

void CloudRenderer::draw(const std::vector<CloudBox> &clouds, const vec3 &cameraPosition,
                         const mat4 &viewProjection, const vec3 &lightColor, float sunHeight,
                         float time) {
  if (clouds.empty())
    return;
  GLint previousProgram, previousVAO, previousActive, previousTex2D, previousTex3D;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActive);
  GLboolean blend = glIsEnabled(GL_BLEND), depthTest = glIsEnabled(GL_DEPTH_TEST);
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  // The depth of what is drawn so far, into a texture of the size of the viewport
  glActiveTexture(GL_TEXTURE5);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex2D);
  glBindTexture(GL_TEXTURE_2D, depth);
  if (viewport[2] != depthWidth || viewport[3] != depthHeight) {
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, viewport[0], viewport[1], viewport[2],
                     viewport[3], 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    depthWidth = viewport[2];
    depthHeight = viewport[3];
  } else {
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], viewport[2], viewport[3]);
  }
  glActiveTexture(GL_TEXTURE6);
  glGetIntegerv(GL_TEXTURE_BINDING_3D, &previousTex3D);
  glBindTexture(GL_TEXTURE_3D, noise);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST); // the shader does its own test, against the copy

  shader->use();
  shader->setInt("depthTexture", 5);
  shader->setInt("noiseTexture", 6);
  mat4 inverse = glm::inverse(viewProjection);
  shader->setMatrix4("inverseViewProjection", (float *)value_ptr(inverse));
  shader->setVector3("cameraPosition", cameraPosition.x, cameraPosition.y, cameraPosition.z);
  shader->setVector3("lightColor", lightColor.x, lightColor.y, lightColor.z);
  shader->setVector2("screenSize", (float)viewport[2], (float)viewport[3]);
  const CloudSettings &c = settings;
  shader->setFloat("time", time);
  shader->setFloat("sunHeight", sunHeight);
  shader->setVector3("octaveWeights", c.weights.x, c.weights.y, c.weights.z);
  shader->setVector3("textureOffset", c.offset.x, c.offset.y, c.offset.z);
  shader->setVector3("drift", c.drift.x, c.drift.y, c.drift.z);
  shader->setVector3("tiling", c.tiling.x, c.tiling.y, c.tiling.z);
  shader->setFloat("threshold", c.threshold);
  shader->setFloat("absorption", c.absorption);
  shader->setFloat("edgeFade", c.edgeFade);
  glBindVertexArray(VAO);
  for (const CloudBox &box : clouds) {
    shader->setVector3("boxMin", box.min.x, box.min.y, box.min.z);
    shader->setVector3("boxMax", box.max.x, box.max.y, box.max.z);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  glBindVertexArray(previousVAO);
  glBindTexture(GL_TEXTURE_3D, previousTex3D);
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, previousTex2D);
  glActiveTexture(previousActive);
  if (depthTest)
    glEnable(GL_DEPTH_TEST);
  if (!blend)
    glDisable(GL_BLEND);
  glUseProgram(previousProgram);
}
