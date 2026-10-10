#include "Campfire.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "CollisionShape.h"

using namespace glm;
using namespace std;

namespace {
// generate_campfire.py: where the logs to sit on lie (round +y from +z) and how far out, how long
const float SEAT_ANGLES[2] = {3.4906585f, 5.5850536f}; // 200 and 320 degrees
const float SEAT_DISTANCE = 1.9f, SEAT_LENGTH = 1.5f, SEAT_R = 0.19f;

// Solid: the fire (too high to step onto) and the two logs to sit on (low: one steps over them)
shared_ptr<CompoundShape> hull() {
  auto shape = make_shared<CompoundShape>();
  shape->add(vec3(Campfire::FIRE_RADIUS + 0.15f, 0.6f, Campfire::FIRE_RADIUS + 0.15f), vec3(0.0f, 0.6f, 0.0f));
  for (float a : SEAT_ANGLES) {
    vec3 centre(std::sin(a) * SEAT_DISTANCE, SEAT_R, std::cos(a) * SEAT_DISTANCE);
    mat3 turn = mat3(rotate(mat4(1.0f), a, vec3(0.0f, 1.0f, 0.0f))); // (its length along x: round the fire)
    shape->add(vec3(SEAT_LENGTH * 0.5f, SEAT_R, SEAT_R), centre, turn);
  }
  return shape;
}

// A SafeSpace round the fire: a cylinder (it shelters within FEAR_RADIUS of it, whatever the height)
class FireShelter : public SafeSpace {
  weak_ptr<const Campfire> fire;

public:
  explicit FireShelter(shared_ptr<const Campfire> f) : SafeSpace(vec3(0.0f), vec3(0.0f)), fire(f) {}
  bool contains(const vec3 &point) const override {
    auto f = fire.lock();
    return f && f->scares(point);
  }
};
} // namespace

Campfire::Campfire(shared_ptr<Model> model, shared_ptr<Model> embers, shared_ptr<Model> flame)
    : GameObject(model, hull()) {
  embersPart = addPart(embers);
  setPartUnlit(embersPart, 2);
  for (size_t &f : flameParts) {
    f = addPart(flame);
    setPartUnlit(f, 2);
  }
  setCullRadius(3.0f);

  ParticleSettings smoke;
  smoke.lifeMin = 2.5f;
  smoke.lifeMax = 4.5f;
  smoke.speedMin = 0.6f;
  smoke.speedMax = 1.2f;
  smoke.spread = 0.25f;
  smoke.sizeStart = 0.15f;
  smoke.sizeEnd = 1.3f;
  smoke.color = vec3(0.30f, 0.29f, 0.28f);
  smoke.alpha = 0.35f;
  smoke.gravity = -0.3f;
  smoke.drag = 0.4f;
  smoke.maxParticles = 120;
  ParticleSettings sparks;
  sparks.lifeMin = 0.6f;
  sparks.lifeMax = 1.4f;
  sparks.speedMin = 1.0f;
  sparks.speedMax = 2.6f;
  sparks.spread = 0.35f;
  sparks.sizeStart = 0.025f;
  sparks.sizeEnd = 0.01f;
  sparks.color = vec3(1.0f, 0.6f, 0.15f);
  sparks.alpha = 1.0f;
  sparks.fadeStart = 0.6f;
  sparks.gravity = -0.5f;
  sparks.drag = 0.6f;
  sparks.maxParticles = 60;
  emitters.push_back(make_shared<ParticleEmitter>(smoke, 401));
  emitters.push_back(make_shared<ParticleEmitter>(sparks, 402));
  emitters[0]->setRate(18.0f);
  emitters[1]->setRate(9.0f);
}

bool Campfire::scares(const vec3 &point) const {
  return length(vec2(point.x - position.x, point.z - position.z)) < FEAR_RADIUS;
}

shared_ptr<SafeSpace> Campfire::shelter(shared_ptr<const Campfire> fire) { return make_shared<FireShelter>(fire); }

void Campfire::placeEmitters() {
  emitters[0]->setPosition(position + vec3(0.0f, 0.9f, 0.0f));
  emitters[1]->setPosition(position + vec3(0.0f, 0.4f, 0.0f));
  emittersPlaced = true;
}

// The flames lick up and down, each at its own pace, leaning a little this way and that
void Campfire::update(double dt) {
  GameObject::update(dt);
  if (!emittersPlaced)
    placeEmitters();
  float t = (float)time;
  for (int i = 0; i < 3; i++) {
    float k = (float)i;
    float tall = 0.85f + 0.18f * std::sin(t * (7.0f + 1.3f * k) + 2.1f * k) + 0.08f * std::sin(t * (17.0f + 2.0f * k) + k);
    float wide = 1.0f - 0.25f * (tall - 0.85f);
    float lean = 0.12f * std::sin(t * (2.3f + 0.7f * k) + 4.0f * k);
    float a = 2.0943951f * k + 0.5f; // (one in front of the next, round the middle)
    vec3 at(std::sin(a) * 0.1f, 0.03f, std::cos(a) * 0.1f);
    mat4 m = glm::translate(mat4(1.0f), at) * glm::rotate(mat4(1.0f), a + 0.4f * std::sin(t * 1.7f + k), vec3(0.0f, 1.0f, 0.0f)) *
             glm::rotate(mat4(1.0f), lean, vec3(0.0f, 0.0f, 1.0f)) *
             glm::scale(mat4(1.0f), vec3(wide, tall * (i == 0 ? 1.0f : 0.8f), wide) * (i == 0 ? 1.0f : 0.8f));
    setPartTransform(flameParts[i], m);
  }
}

void Campfire::getLight(vector<SpotLight> &lights) const {
  float t = (float)time;
  float flicker = 1.0f + 0.18f * std::sin(t * 11.0f) + 0.1f * std::sin(t * 23.0f + 1.0f) + 0.06f * std::sin(t * 37.0f + 2.0f);
  lights.push_back(SpotLight::omni(position + vec3(0.0f, 0.8f, 0.0f), vec3(1.0f, 0.55f, 0.2f) * (1.6f * flicker), LIGHT_RANGE));
}
