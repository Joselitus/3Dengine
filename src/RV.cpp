#include "RV.h"

#include <cmath>
#include <glm/gtx/rotate_vector.hpp>
using namespace glm;

// Strongest acceleration of the RV, units / second^2
#define RV_ACCELERATION 14.0f
// Turning speed at full steering, radians / second
#define TURN_RATE 1.8f
// How quickly the RV reaches the wanted velocity (1 / seconds)
#define RESPONSIVENESS 6.0f

void RV::attachCamera(Camera *camera, float distance, float height) {
  this->camera = camera;
  setYaw(facing);
  camera->attachTo(this, distance, height);
}

void RV::followCamera() {
  if (camera)
    camera->follow();
}

void RV::control(vec2 dir, float up, float cameraYaw) {
  throttle = -dir.y; // W is forward
  steering = dir.x;
  vertical = up;
}

void RV::update(double dt) {
  // It only turns while it moves, like a car
  float speed = length(vec2(velocity.x, velocity.z));
  facing -= steering * TURN_RATE * clamp(speed / 3.0f, 0.0f, 1.0f) * (float)dt;
  setYaw(facing);

  vec3 forward(std::sin(facing), 0.0f, std::cos(facing));
  vec3 wanted = forward * throttle * maxSpeed;
  // Under gravity the RV can't fly: only the stage decides its height
  wanted.y = gravity > 0.0f ? velocity.y : vertical * maxSpeed;
  // Accelerate towards the wanted velocity (or towards a stop)
  maxAcceleration = RV_ACCELERATION;
  steerTowards(wanted, RESPONSIVENESS);

  PlayableCharacter::update(dt);
}
