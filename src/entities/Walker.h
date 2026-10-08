#ifndef WALKER
#define WALKER

#include "PlayableCharacter.h"

class Camera;

// A character on foot. WASD walks relative to where the camera looks (W
// forward, A/D sideways) and it turns to face where it walks. Under gravity
// the stage keeps it on the floor; without it, the `up` input of control()
// flies it up/down (no key is bound to that at the moment).
//
// With a camera distance of 0 the view is first person: the camera sits at
// the given height above the walker's position (its eyes) and the walker
// itself is hidden. With a distance > 0 it is followed from behind.
//
// Holding the run key (setRunning) it goes RUN_FACTOR times faster.
//
// Its flashlight is held a little below and to the right of the eyes and
// points where the camera looks.
class Walker : public PlayableCharacter {
private:
  Camera *camera = nullptr;
  glm::vec2 heading = glm::vec2(0.0f); // wanted direction on x/z, length <= 1
  float vertical = 0.0f;               // only used when not under gravity
  float facing = 0.0f;                 // radians, around +y
  bool running = false;                // the run key is held

public:
  using PlayableCharacter::PlayableCharacter;

  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  void control(glm::vec2 dir, float up, float cameraYaw) override;
  void setRunning(bool running) override { this->running = running; }
  void update(double dt) override;
  void getFlashlight(std::vector<SpotLight> &lights) const override;
  // It climbs ledges up to this high (the sill of the RV's door, with its step)
  float getStepHeight() const override { return 0.7f; }
};

#endif
