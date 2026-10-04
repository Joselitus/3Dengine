#include "Walker.h"

#include <cmath>

#include "Camera.h"

using namespace glm;

// How quickly the walker reaches the speed it wants (see steerTowards)
#define RESPONSIVENESS 12.0f
#define WALK_ACCELERATION 30.0f // units / second^2

void Walker::attachCamera(Camera *camera, float distance, float height) {
  this->camera = camera;
  // In first person the camera is inside the model: don't draw it
  setVisible(distance > 0.0f);
  camera->attachTo(this, distance, height);
}

void Walker::followCamera() {
  if (camera)
    camera->follow();
}

void Walker::control(vec2 dir, float up, float cameraYaw) {
  // dir is in the camera's frame (x right, y backwards): turn it by the
  // camera heading, same convention as Camera::move
  heading = dir == vec2(0.0f) ? vec2(0.0f) : rotate(normalize(dir), cameraYaw);
  vertical = up;
}

void Walker::update(double dt) {
  if (heading != vec2(0.0f)) {
    facing = std::atan2(heading.x, heading.y);
    setYaw(facing);
  }
  vec3 wanted = vec3(heading.x, 0.0f, heading.y) * maxSpeed;
  // Under gravity only the stage decides its height
  wanted.y = gravity > 0.0f ? velocity.y : vertical * maxSpeed;
  maxAcceleration = WALK_ACCELERATION;
  steerTowards(wanted, RESPONSIVENESS);

  PlayableCharacter::update(dt);
}
