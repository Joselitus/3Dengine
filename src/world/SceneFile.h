#ifndef SCENE_FILE
#define SCENE_FILE

#include <glm/glm.hpp>
#include <string>
#include <vector>

// How an object is shaded (see the `unlit` and `breathAmp` uniforms).
enum class Effect {
  Lit,      // Phong with the moonlight, fades into the fog
  Emissive, // flat texture colour, ignores light and fog
  Breathe   // lit, plus procedural breathing in the vertex shader
};

// One static model placed in the world.
struct SceneObject {
  std::string model; // path relative to the assets directory
  glm::vec3 position;
  bool onGround = false; // y was "ground": stand on the floor (y unused)
  float yaw;   // rotation around +y, in radians
  float scale; // uniform
  Effect effect;
};

// Plain description of a scene, as written in a .scene file. It has no
// OpenGL dependencies, so it can be parsed and checked without a window.
// The same format is read by tools/scene_viewer/viewer.js; keep both in sync.
struct SceneFile {
  glm::vec3 moonDir = glm::vec3(0.0f, 1.0f, 0.0f); // normalised
  glm::vec3 lightColor = glm::vec3(1.0f);
  glm::vec3 fogColor = glm::vec3(0.0f);
  std::string sky;    // empty: no sky dome
  std::string floor;  // empty: no floor (nothing to walk on)
  glm::vec3 floorPosition = glm::vec3(0.0f);
  std::string player; // empty: no controllable character
  glm::vec3 playerPosition = glm::vec3(0.0f);
  bool playerOnGround = false;
  float cameraDistance = 4.0f;
  float cameraHeight = 0.8f;
  std::vector<SceneObject> objects;

  // Parses `path`. On error prints "path:line: message" and returns false.
  bool load(const std::string &path);
};

#endif
