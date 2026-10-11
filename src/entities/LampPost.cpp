#include "LampPost.h"

#include "CollisionShape.h"

using namespace glm;
using namespace std;

LampPost::LampPost(shared_ptr<Model> post, shared_ptr<Model> glass)
    : GameObject(post, make_shared<Capsule>(float(RADIUS), float(HEIGHT))) { // (copies: C++11 constexpr)
  glassPart = addPart(glass);
  setCullRadius(8.5f); // (its lamp is 8 m up)
}

vec3 LampPost::lineSpot(int side) const {
  vec3 along = vec3(getRotationMatrix() * vec4(1.0f, 0.0f, 0.0f, 0.0f));
  return position + along * (MIMIC_GAP * (float)side);
}
