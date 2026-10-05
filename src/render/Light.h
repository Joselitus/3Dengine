#ifndef LIGHT
#define LIGHT
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "Shader.h"

// The single (directional) light of the shader: lightPosition is the direction
// towards the light, not a point, and lightColor its colour.
class Light {
private:
  Shader *shader;
  glm::vec3 color;
  glm::vec3 position;

public:
  Light(float r, float g, float b, Shader *shader);
  void moveTo(float x, float y, float z);
  void setColor(float r, float g, float b);
  void update();
};

#endif
