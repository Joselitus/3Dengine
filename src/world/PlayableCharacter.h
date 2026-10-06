#ifndef PLAYABLE_CHARACTER
#define PLAYABLE_CHARACTER

#include <vector>

#include <glm/glm.hpp>

#include "DynamicGameObject.h"
#include "Property.h"
#include "SpotLight.h"

class Camera;

// A character the player drives. Abstract: how it reacts to the controller
// and how the camera follows it is up to each concrete character.
//
// Every playable character carries a flashlight (off at first): the
// headlights key switches it while the player is on foot. Where it shines
// from and where it points is up to each character (getFlashlight).
class PlayableCharacter : public DynamicGameObject {
protected:
  bool flashlightOn = false;

public:
  using DynamicGameObject::DynamicGameObject;

  // The camera follows this character from the given distance and height
  virtual void attachCamera(Camera *camera, float distance, float height) = 0;
  // Keeps the camera on the character; called after the character has moved
  virtual void followCamera() = 0;

  // Input from the controller: dir is the wanted direction in the camera's
  // frame (x right, y backwards), up the vertical input (-1..1) and
  // cameraYaw the camera heading.
  virtual void control(glm::vec2 dir, float up, float cameraYaw) = 0;
  // The run key is held (or let go): a character that can run goes faster
  // while it is (nothing by default)
  virtual void setRunning(bool running) {}

  bool isFlashlightOn() const { return flashlightOn; }
  void setFlashlight(bool on) { flashlightOn = on; }
  void toggleFlashlight() { setFlashlight(!flashlightOn); }
  // Adds the flashlight's beam while it is on (nothing by default)
  virtual void getFlashlight(std::vector<SpotLight> &lights) const {}

  void getProperties(std::vector<Property> &properties) override {
    DynamicGameObject::getProperties(properties);
    properties.push_back(Property::toggle(
        "Linterna", [this]() { return flashlightOn; },
        [this](bool on) { setFlashlight(on); }));
  }
};

#endif
