#include "SafeSpace.h"

#include "GameObject.h"

using namespace glm;

SafeSpace::SafeSpace(std::shared_ptr<const GameObject> carrier, const vec3 &centre, const vec3 &halfSize)
    : owner(carrier), carried(true), centre(centre), halfSize(halfSize) {}

SafeSpace::SafeSpace(const vec3 &centre, const vec3 &halfSize) : centre(centre), halfSize(halfSize) {}

bool SafeSpace::contains(const vec3 &point) const {
  vec3 local = point;
  if (carried) {
    auto carrier = owner.lock();
    if (!carrier)
      return false; // (it went with what carried it)
    Pose pose = carrier->getPose();
    local = transpose(pose.rotation) * (point - pose.position) / pose.scale;
  }
  vec3 d = abs(local - centre);
  return d.x <= halfSize.x && d.y <= halfSize.y && d.z <= halfSize.z;
}
