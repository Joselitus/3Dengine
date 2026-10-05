#include "ImpactDetector.h"

#include <algorithm>

using namespace glm;

static vec2 flat(const vec3 &v) { return vec2(v.x, v.z); }

void ImpactDetector::onCollision(float speedBefore, float speedAfter, const vec3 &heading,
                                 const vec3 &away) {
  vec2 h = flat(heading), a = flat(away);
  if (length(h) < 1e-4f || length(a) < 1e-4f)
    return; // a push straight up (the floor) or no heading: not a frontal hit
  // Frontal: what it hit is ahead, so the push away from it points back
  if (dot(normalize(a), normalize(h)) > -params.frontalCos)
    return;
  if (speedBefore < params.minSpeed)
    return;
  if (!running) { // a collision during the window does not restart it
    running = true;
    elapsed = 0.0f;
    speedAtImpact = speedBefore;
  }
  check(speedAfter); // most of the drop is in the hit itself
}

void ImpactDetector::update(float dt, float forwardSpeed) {
  if (!running)
    return;
  elapsed += dt;
  check(forwardSpeed);
  if (elapsed >= params.window)
    running = false;
}

void ImpactDetector::check(float speedNow) {
  if (!running)
    return;
  float drop = speedAtImpact - speedNow;
  // as fast as `decel` over the time since the hit, and not just a scratch
  if (drop >= std::max(params.minDrop, params.decel * elapsed)) {
    wasViolent = true;
    running = false;
  }
}
