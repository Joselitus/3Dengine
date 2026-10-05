#ifndef GAME_STAGE
#define GAME_STAGE

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Interactable.h"
#include "SpotLight.h"
#include "PlayableCharacter.h"
#include "Stage.h"

// Light and colours of a map. The light is a far point light in `lightDir`
// (the sun or the moon); `horizon` is the clear colour and the fog colour.
struct Environment {
  glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f, 0.8f, -0.5f));
  glm::vec3 lightColor = glm::vec3(1.0f);
  glm::vec3 horizon = glm::vec3(0.5f, 0.7f, 0.9f);
  // For a procedural sky (setSky with unlit 3): colour straight up, direction
  // to the sun, and how visible the stars are (0..1)
  glm::vec3 skyZenith = glm::vec3(0.2f, 0.4f, 0.8f);
  glm::vec3 sunDir = glm::vec3(0.0f, 1.0f, 0.0f);
  float starAlpha = 0.0f;
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
  float cameraYaw = 0.0f;      // where the view starts looking (0 = towards -z)
  std::vector<Interactable *> interactables; // owned by the stage
  bool playerChanged = false; // see takePlayerChange()

  explicit GameStage(FloorMode mode) : Stage(mode) {}

  // Dynamic objects stay on the floor (override for other rules)
  void apply(DynamicGameObject &object, double dt) override {
    collideWithFloor(object, dt);
  }

  // Hands the controls and the camera to another character (e.g. when the
  // player gets into a vehicle): the main loop notices it (takePlayerChange)
  // and attaches the controller to the new player with this distance (0 =
  // first person) and height
  void setPlayer(std::shared_ptr<PlayableCharacter> newPlayer, float distance,
                 float height, float yaw = 0.0f) {
    player = newPlayer;
    cameraDistance = distance;
    cameraHeight = height;
    cameraYaw = yaw;
    playerChanged = true;
  }

  // A sky dome model (drawn unlit; see shader.frag). unlit 1 = textured with
  // twinkling stars, 3 = painted by the shader from the Environment (sun,
  // stars, colours), so it can follow the time of day
  void setSky(std::shared_ptr<Model> model, int unlit = 1);
  // Ground height at (x, z), or `fallback` where there is no floor
  float groundAt(float x, float z, float fallback) const;

public:
  const Environment &getEnvironment() const { return environment; }

  // True once after the player has changed (setPlayer): then the controller
  // has to be attached to getPlayer() again
  bool takePlayerChange() {
    bool changed = playerChanged;
    playerChanged = false;
    return changed;
  }
  // The "leave the vehicle" key was pressed (no panel open): a map where the
  // player can drive something gives the controls back to a character on foot
  virtual void leaveVehicle() {}
  // The headlights key was pressed: a map with a vehicle the player is
  // driving turns its lights on or off
  virtual void toggleHeadlights() {}
  // The camera key was pressed: a map with a vehicle the player is driving
  // changes the point of view (inside it / from behind)
  virtual void toggleVehicleCamera() {}
  // Adds the spot lights that are on right now (the shader takes the first
  // few; see the main loop)
  virtual void getSpotLights(std::vector<SpotLight> &lights) const {}
  // False while the player can't use objects (e.g. while driving)
  virtual bool interactionsEnabled() const { return true; }
  std::shared_ptr<PlayableCharacter> getPlayer() const { return player; }
  float getCameraDistance() const { return cameraDistance; }
  float getCameraHeight() const { return cameraHeight; }
  float getCameraYaw() const { return cameraYaw; }
  const std::vector<Interactable *> &getInteractables() const {
    return interactables;
  }

  // Draws the sky around the camera (without depth, so it stays behind
  // everything) and then the stage
  void render(Shader *shader, const glm::vec3 &cameraPosition, double time);
};

#endif
