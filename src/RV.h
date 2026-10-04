#ifndef RV_CHARACTER
#define RV_CHARACTER

#include "Camera.h"
#include "PlayableCharacter.h"

// The RV: W/S drive it along its own heading and A/D turn it, so the camera
// can orbit freely without turning the mesh. The camera follows it from
// behind, orbiting around it.
class RV : public PlayableCharacter {
private:
  Camera *camera = nullptr;
  float facing = 3.14159265f; // heading, radians (pi = towards -z)
  float throttle = 0.0f;      // -1 (reverse) .. 1 (forward)
  float steering = 0.0f;      // -1 (left) .. 1 (right)
  float vertical = 0.0f;      // only used when not under gravity

public:
  using PlayableCharacter::PlayableCharacter;

  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  // Stores the input; the camera heading is ignored on purpose
  void control(glm::vec2 dir, float up, float cameraYaw) override;
  void update(double dt) override;
};

#endif
