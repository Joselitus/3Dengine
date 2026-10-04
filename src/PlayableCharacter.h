#ifndef PLAYABLE_CHARACTER
#define PLAYABLE_CHARACTER

#include <glm/glm.hpp>

#include "DynamicGameObject.h"

class Camera;

// A character the player drives. Abstract: how it reacts to the controller
// and how the camera follows it is up to each concrete character.
class PlayableCharacter : public DynamicGameObject {
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
};

#endif
