#include "LampStalker.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "CollisionShape.h"
#include "LampPost.h"
#include "NetBuffer.h"

using namespace glm;
using namespace std;

namespace {
const float PI = 3.14159265f;
// generate_lamppost.py: the footing's height, how far the feet are from its middle, and the top of
// a foot's leg over its sole
const float FOOTING_HEIGHT = 0.6f, FOOT_RING = 0.13f, LEG_TOP = 0.5f;
// Feet out, the post stands this high on them (their legs still reach into the footing); feet in,
// their soles are this high (all of them inside the footing)
const float LIFT = 0.4f, TUCKED = FOOTING_HEIGHT - LEG_TOP - 0.05f;
const float STRIDE = 0.1f;      // m, how far a foot swings forward and back
const float STEP_LIFT = 0.07f;  // m, how high it lifts
const float STEP_RATE = 14.0f;  // rad of its step cycle per metre run
const float LEAN = 0.12f;       // rad, forward at full speed

float angleTo(float a, float b) {
  float d = std::fmod(b - a + PI, 2.0f * PI);
  if (d < 0.0f)
    d += 2.0f * PI;
  return d - PI;
}
} // namespace

LampStalker::LampStalker(shared_ptr<Model> post, shared_ptr<Model> glass, shared_ptr<Model> foot)
    : DynamicGameObject(post, make_shared<Capsule>(float(LampPost::RADIUS), float(LampPost::HEIGHT))) {
  glassPart = addPart(glass);
  for (size_t &f : feet)
    f = addPart(foot);
  setGravity(25.0f);
  setMass(200.0f);
  setMaxSpeed(RUN_SPEED);
  setCullRadius(8.5f);
  pose(0.0);
}

void LampStalker::setYaw(float y) {
  yaw = y;
  rotation = rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f));
  if (state == State::Pole)
    home.yaw = yaw;
}

void LampStalker::teleport(const vec3 &where) {
  DynamicGameObject::teleport(where);
  home = {where, yaw};
}

void LampStalker::enter(State next) {
  state = next;
  hasSpot = false;
  replanIn = 0.0f;
  if (next == State::Dead)
    setCollidable(false);
}

// The nearest player alive within LOSE_RANGE
bool LampStalker::findTarget(Victim &victim) const {
  if (!victims)
    return false;
  bool found = false;
  float best = LOSE_RANGE;
  for (const Victim &v : victims()) {
    float d = length(vec2(v.position.x - position.x, v.position.z - position.z));
    if (d < best) {
      best = d;
      victim = v;
      found = true;
    }
  }
  return found;
}

// The hiding place nearest to IDEAL_DISTANCE from him, among those KEEP_NEAR..KEEP_FAR away (or, if
// none is, the one that comes closest to that); none if there are no posts
void LampStalker::chooseSpot(const vec3 &from) {
  hasSpot = false;
  if (!spots)
    return;
  float best = 1e9f;
  for (const Spot &s : spots()) {
    float d = length(vec2(s.position.x - from.x, s.position.z - from.z));
    float score = std::fabs(d - IDEAL_DISTANCE);
    if (d < KEEP_NEAR || d > KEEP_FAR)
      score += 1000.0f; // (only if there is nothing better)
    if (score < best) {
      best = score;
      spot = s;
      hasSpot = true;
    }
  }
}

void LampStalker::walkTo(const vec3 &where, float speed, double dt) {
  vec3 to = where - position;
  to.y = 0.0f;
  float d = length(to);
  if (d > 0.05f)
    yaw += clamp(angleTo(yaw, std::atan2(to.x, to.z)), -TURN_RATE * (float)dt, TURN_RATE * (float)dt);
  setYaw(yaw);
  // (it can only go as fast as its feet are out; and it slows down to stop where it wants)
  float pace = speed * extend * std::min(1.0f, d / 1.5f);
  vec3 way = d > 0.05f ? to / d : vec3(0.0f);
  // something in the way (the RV, a post...): it goes round it, sideways for a moment
  float going = length(vec2(velocity.x, velocity.z));
  stuck = extend >= 1.0f && pace > 1.0f && going < 1.0f ? stuck + (float)dt : 0.0f;
  if (stuck > 0.6f && detour <= 0.0f) {
    detour = DETOUR_TIME;
    detourSide = -detourSide;
    stuck = 0.0f;
  }
  if (detour > 0.0f) {
    detour -= (float)dt;
    way = normalize(way * 0.3f + vec3(-way.z, 0.0f, way.x) * detourSide);
  }
  vec3 want = way * pace;
  steerTowards(vec3(want.x, velocity.y, want.z), 12.0f);
  acceleration.y = 0.0f;
}

void LampStalker::standStill() {
  velocity.x = velocity.z = 0.0f;
  acceleration.x = acceleration.z = 0.0f;
}

void LampStalker::update(double dt) {
  float dtf = (float)dt;
  if (replica) { // (the server moves it: here its feet step as fast as it goes)
    GameObject::update(dt);
    pose(dt);
    return;
  }
  bool moving = false;
  Victim victim;
  switch (state) {
  case State::Pole:
    standStill();
    if (victims) // (somebody passes in front of it: in reach, on the side its arm points to)
      for (const Victim &v : victims()) {
        vec2 to(v.position.x - position.x, v.position.z - position.z);
        float d = length(to);
        if (d < TRIGGER_RANGE && d > 0.01f && dot(to / d, vec2(std::sin(yaw), std::cos(yaw))) > FRONT_DOT) {
          target = v.id;
          enter(v.sheltered ? State::Hide : State::Chase);
          break;
        }
      }
    break;
  case State::Chase:
    if (!findTarget(victim)) {
      enter(State::Return);
      break;
    }
    if (victim.sheltered) {
      enter(State::Hide);
      break;
    }
    target = victim.id;
    moving = true;
    walkTo(victim.position, RUN_SPEED, dt);
    if (length(vec2(victim.position.x - position.x, victim.position.z - position.z)) < KILL_DISTANCE &&
        std::fabs(victim.position.y - position.y) < 1.5f) {
      if (kill)
        kill(victim.id);
      enter(State::Return); // (its job is done: back to its place in the row)
    }
    break;
  case State::Hide: {
    if (!findTarget(victim)) {
      enter(State::Return);
      break;
    }
    float d = length(vec2(victim.position.x - position.x, victim.position.z - position.z));
    if (!victim.sheltered && d < CHASE_AGAIN) {
      enter(State::Chase);
      break;
    }
    if ((replanIn -= dtf) <= 0.0f) {
      replanIn = REPLAN_TIME;
      float fromSpot = hasSpot ? length(vec2(spot.position.x - victim.position.x, spot.position.z - victim.position.z)) : 0.0f;
      if (!hasSpot || fromSpot < KEEP_NEAR || fromSpot > KEEP_FAR)
        chooseSpot(victim.position);
    }
    if (!hasSpot) { // (no posts to hide among: it just stands)
      standStill();
      break;
    }
    if (length(vec2(spot.position.x - position.x, spot.position.z - position.z)) > 0.3f) {
      moving = true;
      walkTo(spot.position, RUN_SPEED, dt);
    } else { // there: one more post of the row, turned like them
      standStill();
      yaw += clamp(angleTo(yaw, spot.yaw), -TURN_RATE * dtf, TURN_RATE * dtf);
      setYaw(yaw);
    }
    break;
  }
  case State::Return:
    if (findTarget(victim) && length(vec2(victim.position.x - position.x, victim.position.z - position.z)) <
                                  TRIGGER_RANGE) {
      target = victim.id; // (somebody is there again on the way)
      enter(victim.sheltered ? State::Hide : State::Chase);
      break;
    }
    if (length(vec2(home.position.x - position.x, home.position.z - position.z)) > 0.3f) {
      moving = true;
      walkTo(home.position, RUN_SPEED, dt);
    } else {
      standStill();
      yaw += clamp(angleTo(yaw, home.yaw), -TURN_RATE * dtf, TURN_RATE * dtf);
      setYaw(yaw);
      if (std::fabs(angleTo(yaw, home.yaw)) < 0.01f && extend <= 0.0f)
        enter(State::Pole);
    }
    break;
  case State::Dead:
    standStill();
    fall = std::min(1.0f, fall + dtf / FALL_TIME);
    break;
  }
  // its feet go out before it moves, and in when it stops
  extend = clamp(extend + (moving ? 1.0f : -1.0f) * FEET_RATE * dtf, 0.0f, 1.0f);
  DynamicGameObject::update(dt);
  pose(dt);
}

// Standing, it is a post (the footing on the ground, the feet inside it); out, it rises on its feet,
// which step very fast as it goes, and leans forward; dead, it lies on the ground
void LampStalker::pose(double dt) {
  float speed = length(vec2(velocity.x, velocity.z));
  phase = std::fmod(phase + speed * STEP_RATE * (float)dt, 2.0f * PI);
  float run = clamp(speed / RUN_SPEED, 0.0f, 1.0f);
  float lift = LIFT * extend + 0.03f * extend * run * std::fabs(std::sin(phase));
  mat4 lean = rotate(mat4(1.0f), LEAN * run + fall * 0.5f * PI, vec3(1.0f, 0.0f, 0.0f));
  mat4 body = lean * glm::translate(mat4(1.0f), vec3(0.0f, lift, 0.0f));
  setPartTransform(0, body);
  setPartTransform(glassPart, body);
  for (int k = 0; k < 4; k++) {
    float a = PI * 0.25f + PI * 0.5f * k;
    float p = phase + (k % 2 == 0 ? 0.0f : PI); // (diagonal pairs in turn)
    float swing = std::sin(p) * STRIDE * extend * run;
    float up = std::max(0.0f, std::cos(p)) * STEP_LIFT * extend * run;
    float sole = mix(TUCKED, 0.0f, extend) + up;
    setPartTransform(feet[k], lean * glm::translate(mat4(1.0f), vec3(std::sin(a) * FOOT_RING, sole, std::cos(a) * FOOT_RING + swing)));
  }
}

void LampStalker::takeDamage(float amount, const vec3 &direction, const Stage &stage) {
  if (state == State::Dead || replica)
    return;
  health -= amount;
  if (health <= 0.0f)
    enter(State::Dead);
}

// u8 state, f32 feet out, f32 fallen
void LampStalker::writeNetState(NetWriter &out) const {
  out.u8((uint8_t)state);
  out.f32(extend);
  out.f32(fall);
}

void LampStalker::readNetState(NetReader &in) {
  uint8_t s = in.u8();
  float e = in.f32(), f = in.f32();
  if (!in.isOk() || s > (uint8_t)State::Dead)
    return;
  state = (State)s;
  extend = e;
  fall = f;
}

void LampStalker::describe(vector<string> &lines) const {
  DynamicGameObject::describe(lines);
  const char *names[] = {"es un poste", "te persigue", "se esconde entre los postes", "vuelve a su sitio", "muerto"};
  lines.push_back(string("Poste vivo: ") + names[(int)state]);
}
