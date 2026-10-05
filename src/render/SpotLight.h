#ifndef SPOT_LIGHT
#define SPOT_LIGHT

#include <glm/glm.hpp>

// A cone of light (e.g. a headlight) in world space. The shader lights what
// is inside the cone, brightest in the middle (inside innerCos) and fading
// out to nothing at the edge (outerCos), and fading with the distance up to
// `range`. Maps list the ones that are on (GameStage::getSpotLights) and the
// main loop hands them to the shader every frame.
struct SpotLight {
  glm::vec3 position;
  glm::vec3 direction; // unit length: where it points
  glm::vec3 color;
  float innerCos = 0.93f; // cosine of the half angle of the full-bright cone
  float outerCos = 0.80f; // cosine of the half angle where it fades out
  float range = 45.0f;    // metres

  // A lamp that shines all around (a bulb, the glow of a dial): no cone, only the
  // fall-off with the distance up to `range`
  static SpotLight omni(const glm::vec3 &position, const glm::vec3 &color, float range) {
    SpotLight light;
    light.position = position;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f); // (does not matter)
    light.color = color;
    light.innerCos = -1.5f; // the shader's cone: below -1 everything is inside it
    light.outerCos = -2.0f;
    light.range = range;
    return light;
  }
};

#endif
