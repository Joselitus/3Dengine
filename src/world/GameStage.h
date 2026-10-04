#ifndef GAME_STAGE
#define GAME_STAGE

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Interactable.h"
#include "PlayableCharacter.h"
#include "Stage.h"

// Light and colours of a map. The light is a far point light in `lightDir`
// (the sun or the moon); `horizon` is the clear colour and the fog colour.
struct Environment {
  glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f, 0.8f, -0.5f));
  glm::vec3 lightColor = glm::vec3(1.0f);
  glm::vec3 horizon = glm::vec3(0.5f, 0.7f, 0.9f);
};

// A playable map: a Stage that also knows everything the game needs to run
// it, so maps can be swapped at runtime (see MapSelector): its environment,
// an optional sky dome, the player and the camera it wants, and the objects
// the player can use. Abstract like Stage: each map fills these in its
// constructor (see TestStage in test.cpp, SceneStage).
class GameStage : public Stage {
protected:
  Environment environment;
  std::shared_ptr<GameObject> sky; // drawn around the camera, behind all
  std::shared_ptr<PlayableCharacter> player;
  float cameraDistance = 0.0f; // 0 = first person
  float cameraHeight = 1.6f;   // above the player's position (its feet)
  std::vector<Interactable *> interactables; // owned by the stage

  explicit GameStage(FloorMode mode) : Stage(mode) {}

  // Dynamic objects stay on the floor (override for other rules)
  void apply(DynamicGameObject &object, double dt) override {
    collideWithFloor(object, dt);
  }

  // A sky dome model (drawn unlit, with twinkling stars; see shader.frag)
  void setSky(std::shared_ptr<Model> model);
  // Ground height at (x, z), or `fallback` where there is no floor
  float groundAt(float x, float z, float fallback) const;

public:
  const Environment &getEnvironment() const { return environment; }
  std::shared_ptr<PlayableCharacter> getPlayer() const { return player; }
  float getCameraDistance() const { return cameraDistance; }
  float getCameraHeight() const { return cameraHeight; }
  const std::vector<Interactable *> &getInteractables() const {
    return interactables;
  }

  // Draws the sky around the camera (without depth, so it stays behind
  // everything) and then the stage
  void render(Shader *shader, const glm::vec3 &cameraPosition, double time);
};

#endif
