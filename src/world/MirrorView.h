#ifndef MIRROR_VIEW
#define MIRROR_VIEW

#include <glm/glm.hpp>

// Where a rear-view mirror's camera is and which way it looks (see GameStage::rearMirror): at the
// glass, looking where the driver's eye would see in it (his line of sight reflected in the
// glass). `up` is roughly the vehicle's up; `fov` is vertical, in degrees; `aspect` is the
// glass's width over its height.
struct MirrorView {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  float fov = 30.0f;
  float aspect = 0.5f;
};

#endif
