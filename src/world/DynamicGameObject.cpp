#include "DynamicGameObject.h"
using namespace glm;

void DynamicGameObject::steerTowards(const vec3 &wantedVelocity,
                                     float responsiveness) {
  vec3 a = (wantedVelocity - velocity) * responsiveness;
  float len = length(a);
  if (len > maxAcceleration)
    a *= maxAcceleration / len;
  acceleration = a;
}

void DynamicGameObject::update(double dt) {
  GameObject::update(dt);
  velocity += (acceleration - vec3(0.0f, gravity, 0.0f)) * (float)dt;
  // maxSpeed limits the horizontal speed only, so falling is not capped
  float speed = length(vec2(velocity.x, velocity.z));
  if (speed > maxSpeed) {
    velocity.x *= maxSpeed / speed;
    velocity.z *= maxSpeed / speed;
  }
  position += velocity * (float)dt;
}
