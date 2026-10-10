#include "Shade.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "NetBuffer.h"

using namespace glm;
using namespace std;

namespace {
const float PI = 3.14159265f;
const float TURN_RATE = 1.2f;   // rad/s, as it turns to face the player
const float SWAY_RATE = 0.9f;   // rad/s of its phase
const float SWAY = 0.035f;      // rad, side to side
const float CLOUD_HEIGHT = 0.8f; // m over its feet, the middle of the smoke
const int PUFF = 90;            // smoke particles all at once when it melts

float angleTo(float a, float b) {
  float d = std::fmod(b - a + PI, 2.0f * PI);
  if (d < 0.0f)
    d += 2.0f * PI;
  return d - PI;
}
} // namespace

Shade::Shade(const vector<shared_ptr<Model>> &shapes, unsigned seed)
    : DynamicGameObject(shapes[0], make_shared<Capsule>(0.5f, 1.8f)), random(seed) {
  for (size_t i = 1; i < shapes.size(); i++)
    addMesh(shapes[i]);
  setGravity(0.0f);
  setCollidable(false); // (it goes through anything)
  setVisible(false);
  // dark smoke: wide, slow puffs that swell, rise a little and fade
  ParticleSettings puff;
  puff.lifeMin = 3.0f;
  puff.lifeMax = 5.5f;
  puff.speedMin = 0.3f;
  puff.speedMax = 1.4f;
  puff.spread = 1.5f;
  puff.sizeStart = 0.35f;
  puff.sizeEnd = 1.5f;
  puff.color = vec3(0.10f, 0.10f, 0.11f);
  puff.alpha = 0.55f;
  puff.fadeStart = 0.3f;
  puff.gravity = -0.15f;
  puff.drag = 1.2f;
  puff.maxParticles = 220;
  smoke = make_shared<ParticleEmitter>(puff, 500 + seed);
}

void Shade::enter(State next) {
  state = next;
  stateTime = 0.0f;
}

void Shade::appear() {
  vec3 at;
  int who = -1;
  if (!spot || !spot(at, who)) {
    awayFor = RETRY_TIME;
    return;
  }
  variant = std::uniform_int_distribution<int>(0, VARIANTS - 1)(random);
  target = who;
  position = at;
  velocity = vec3(0.0f);
  vec3 player;
  if (where && where(target, player))
    yaw = std::atan2(player.x - at.x, player.z - at.z);
  phase = std::uniform_real_distribution<float>(0.0f, 2.0f * PI)(random);
  enter(State::Following);
}

void Shade::dissolve() {
  enter(State::Smoke);
  smoke->setPosition(position + vec3(0.0f, CLOUD_HEIGHT, 0.0f));
  smoke->burst(PUFF);
}

void Shade::goAway() {
  enter(State::Away);
  awayFor = std::uniform_real_distribution<float>(AWAY_MIN, AWAY_MAX)(random);
  target = -1;
}

void Shade::show() {
  setMesh((size_t)variant);
  float size = 1.0f;
  if (state == State::Smoke)
    size = std::max(0.0f, 1.0f - stateTime / SHRINK_TIME);
  setScale(size);
  setVisible(state == State::Following || (state == State::Smoke && size > 0.0f));
}

// The smoke goes on coming out (less and less) for a while after the first puff
void Shade::updateSmoke(double dt) {
  float rate = 0.0f;
  if (state == State::Smoke) {
    float left = 1.0f - stateTime / (CLOUD_TIME - 4.0f); // (the last puffs live ~4 s more)
    rate = left > 0.0f ? 25.0f * left : 0.0f;
  }
  smoke->setPosition(position + vec3(0.0f, CLOUD_HEIGHT, 0.0f));
  smoke->setRate(rate);
}

void Shade::update(double dt) {
  GameObject::update(dt);
  float dtf = (float)dt;
  stateTime += dtf;
  if (replica) { // (the server moves it; here it shows, and smokes)
    show();
    updateSmoke(dt);
    return;
  }
  bool night = isNight ? isNight() : true;
  switch (state) {
  case State::Away:
    awayFor -= dtf;
    if (night && awayFor <= 0.0f)
      appear();
    break;
  case State::Following: {
    vec3 player;
    if (!night) { // dawn: it melts away
      dissolve();
      break;
    }
    if (!where || !where(target, player)) {
      goAway(); // (he is dead or gone: it goes too)
      break;
    }
    vec2 to(player.x - position.x, player.z - position.z);
    float distance = length(to);
    if (distance <= VANISH_DISTANCE) {
      dissolve();
      break;
    }
    if (distance >= LOSE_DISTANCE) {
      goAway();
      break;
    }
    // it keeps its distance: nearer if he is far, back if he comes (slower than he walks)
    float step = 0.0f;
    if (distance > KEEP_FAR)
      step = std::min(WALK_SPEED * dtf, distance - KEEP_FAR);
    else if (distance < KEEP_NEAR)
      step = -std::min(WALK_SPEED * dtf, KEEP_NEAR - distance);
    position.x += to.x / distance * step;
    position.z += to.y / distance * step;
    float ground = position.y;
    if (floorHeight)
      floorHeight(position.x, position.z, ground);
    position.y = ground;
    yaw += clamp(angleTo(yaw, std::atan2(to.x, to.y)), -TURN_RATE * dtf, TURN_RATE * dtf);
    break;
  }
  case State::Smoke:
    if (stateTime >= CLOUD_TIME)
      goAway();
    break;
  }
  velocity = vec3(0.0f);
  phase += SWAY_RATE * dtf;
  rotation = rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f)) *
             rotate(mat4(1.0f), SWAY * std::sin(phase), vec3(0.0f, 0.0f, 1.0f)) *
             rotate(mat4(1.0f), 0.5f * SWAY * std::sin(0.7f * phase), vec3(1.0f, 0.0f, 0.0f));
  show();
  updateSmoke(dt);
}

// u8 state, u8 variant, f32 seconds in that state
void Shade::writeNetState(NetWriter &out) const {
  out.u8((uint8_t)state);
  out.u8((uint8_t)variant);
  out.f32(stateTime);
}

void Shade::readNetState(NetReader &in) {
  uint8_t s = in.u8(), v = in.u8();
  float time = in.f32();
  if (!in.isOk() || s > (uint8_t)State::Smoke || v >= VARIANTS)
    return;
  State next = (State)s;
  if (next == State::Smoke && state != State::Smoke) { // (it melts: the first puff, here too)
    smoke->setPosition(position + vec3(0.0f, CLOUD_HEIGHT, 0.0f));
    smoke->burst(PUFF);
  }
  state = next;
  variant = v;
  stateTime = time;
}

void Shade::describe(vector<string> &lines) const {
  DynamicGameObject::describe(lines);
  const char *states[] = {"no esta", "te sigue", "humo"};
  const char *shapes[] = {"lobo", "ciervo", "felino", "reptante", "alto", "jorobado", "hombre cabra", "hombre cuervo"};
  lines.push_back(string("Sombra (") + shapes[variant] + "): " + states[(int)state]);
}
