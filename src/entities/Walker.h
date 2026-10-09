#ifndef WALKER
#define WALKER

#include "PlayableCharacter.h"
#include <functional>

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
  // Where the player looks (his camera's angles), when this walker has no camera of its own: the
  // server's, and those of the other players on a client. The flashlight points there.
  bool hasLook = false;
  float lookYaw = 0.0f, lookPitch = 0.0f;
  std::function<void()> damageCallback; // what happens when something shoots him (the stage kills him)

public:
  using PlayableCharacter::PlayableCharacter;

  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  void control(glm::vec2 dir, float up, float cameraYaw) override;
  void setRunning(bool running) override { this->running = running; }
  void setLook(float yaw, float pitch) {
    hasLook = true;
    lookYaw = yaw;
    lookPitch = pitch;
  }
  float getLookYaw() const { return lookYaw; }
  float getLookPitch() const { return lookPitch; }
  // For the other players: whether the flashlight is on, and where he looks
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  void update(double dt) override;
  void getFlashlight(std::vector<SpotLight> &lights) const override;
  // It climbs ledges up to this high (the sill of the RV's door, with its step)
  float getStepHeight() const override { return 0.7f; }
  // A shot (the ship's ray) kills him: the stage decides what that means (setDamageCallback)
  void setDamageCallback(std::function<void()> callback) { damageCallback = callback; }
  void takeDamage(float amount, const glm::vec3 &direction, const Stage &stage) override {
    if (damageCallback)
      damageCallback();
  }
};

#endif
