#include "DynamicGameObject.h"

#include <cmath>

#include "TextFormat.h"
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
  // Drag on the ground plane (not on falling): exponential, so it does not
  // depend on the frame rate
  if (drag > 0.0f) {
    float keep = std::exp(-drag * (float)dt);
    velocity.x *= keep;
    velocity.z *= keep;
  }
  position += velocity * (float)dt;
}

void DynamicGameObject::describe(std::vector<std::string> &lines) const {
  GameObject::describe(lines);
  lines.push_back(textFormat("Velocidad: %s  %.2f m/s",
                             textOf(velocity).c_str(), length(velocity)));
  lines.push_back("Aceleracion: " + textOf(acceleration));
  lines.push_back(textFormat("Masa: %.0f kg  Gravedad: %.1f m/s2", getMass(),
                             gravity));
  lines.push_back(textFormat("Vel. max: %.1f m/s  Rozamiento: %.1f /s", maxSpeed,
                             drag));
  lines.push_back(std::string("En el suelo: ") + (grounded ? "si" : "no"));
}
