#include "Gnome.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

using namespace glm;
using namespace std;

namespace {
const float PI = 3.14159265f;
// His joints (generate_gnome.py: HIP_L, SHOULDER_R; the right leg mirrors the left)
const vec3 HIP(0.045f, 0.22f, 0.0f), SHOULDER(-0.095f, 0.43f, 0.0f);
const float LEG_SWING = 0.9f;   // radians, at full stride
const float ARM_RAISED = -1.7f; // the knife arm out in front of him (negative: forward)

float angleTo(float a, float b) {
  float d = std::fmod(b - a + PI, 2.0f * PI);
  if (d < 0.0f)
    d += 2.0f * PI;
  return d - PI;
}

// A turn of `angle` about the x axis through `pivot`
mat4 swingAbout(const vec3 &pivot, float angle) {
  mat4 m = glm::translate(mat4(1.0f), pivot);
  m = rotate(m, angle, vec3(1.0f, 0.0f, 0.0f));
  return glm::translate(m, -pivot);
}
} // namespace

Gnome::Gnome(shared_ptr<Model> body, const vector<shared_ptr<Model>> &faces, shared_ptr<Model> eyes,
             shared_ptr<Model> arm, shared_ptr<Model> knife, shared_ptr<Model> legLeft,
             shared_ptr<Model> legRight)
    : DynamicGameObject(body, make_shared<Capsule>(0.2f, 0.84f)) {
  for (int i = 0; i < FACES && i < (int)faces.size(); i++)
    facePart[i] = addPart(faces[i]);
  eyesPart = addPart(eyes, 2); // (white, glowing)
  armPart = addPart(arm);
  knifePart = addPart(knife);
  legPart[0] = addPart(legLeft);
  legPart[1] = addPart(legRight);
  setGravity(25.0f);
  setMass(20.0f);
  setMaxSpeed(CHASE_SPEED);
  setMaxAcceleration(30.0f);
  showFace();
  setPartVisible(knifePart, false);
}

// Only the face he has now, and the knife once he is armed
void Gnome::showFace() {
  for (int i = 0; i < FACES; i++)
    setPartVisible(facePart[i], i == face);
  setPartVisible(eyesPart, face == LAST_FACE);
  setPartVisible(knifePart, armed);
  setVisible(!dead);
}

void Gnome::teleport(const vec3 &where) {
  DynamicGameObject::teleport(where);
  lastPosition = where;
}

void Gnome::takeDamage(float amount, const vec3 &direction, const Stage &stage) {
  if (dead || replica)
    return;
  if (--health <= 0) {
    dead = true;
    armed = false;
    velocity = vec3(0.0f);
    setCollidable(false);
    showFace();
  }
}

// Does anybody look at his face? From in front of him (he is seen from where the face is), within
// range, with the face near the middle of the player's view
bool Gnome::isLookedAt() const {
  if (!viewers)
    return false;
  vector<Viewer> all;
  viewers(all);
  vec3 head = position + vec3(0.0f, FACE_HEIGHT, 0.0f);
  float heading = getHeading();
  vec3 front(std::sin(heading), 0.0f, std::cos(heading));
  float cone = std::cos(radians(LOOK_CONE));
  for (const Viewer &v : all) {
    vec3 to = head - v.eye;
    float d = length(to);
    if (d < 0.05f || d > LOOK_RANGE)
      continue;
    to /= d;
    if (dot(to, v.direction) < cone)
      continue;
    // the face is turned to him: he stands on the side of the gnome it looks to
    if (dot(-to, front) < 0.2f)
      continue;
    if (clearView && !clearView(v.eye, head))
      continue;
    return true;
  }
  return false;
}

// The fifth look: the knife comes out
void Gnome::arm() {
  armed = true;
  showFace();
}

void Gnome::update(double dt) {
  if (replica) {
    updateReplica(dt);
    return;
  }
  float dtf = (float)dt;
  if (dead) {
    GameObject::update(dt);
    return;
  }
  if (!armed) {
    // Standing still, he counts the looks
    if (isLookedAt()) {
      awayTime = 0.0f;
      lookTime += dtf;
      if (face == LAST_FACE) {
        if (lookTime >= FINAL_HOLD)
          arm();
      } else if (lookTime >= LOOK_HOLD) {
        pendingChange = true;
      }
    } else {
      lookTime = 0.0f;
      awayTime += dtf;
      if (pendingChange && awayTime >= AWAY_DELAY) {
        pendingChange = false;
        face = face < LAST_FACE ? face + 1 : LAST_FACE;
        showFace();
      }
    }
  }

  Victim victim;
  bool chasing = armed && findVictim && findVictim(position, victim) &&
                 length(vec2(victim.position.x - position.x, victim.position.z - position.z)) < CHASE_RANGE;
  if (chasing) {
    vec3 to = victim.position - position;
    to.y = 0.0f;
    float d = length(to);
    float heading = getHeading();
    if (d > 0.01f)
      heading += clamp(angleTo(heading, std::atan2(to.x, to.z)), -TURN_RATE * dtf, TURN_RATE * dtf);
    setYaw(heading);
    vec3 want = d > 0.05f ? to / d * CHASE_SPEED : vec3(0.0f);
    steerTowards(vec3(want.x, velocity.y, want.z), 12.0f);
    acceleration.y = 0.0f;
    if (d < KILL_DISTANCE && fabsf(victim.position.y - position.y) < 1.3f && caught) {
      caught(victim.id);
      // His job is done: he vanishes
      dead = true;
      armed = false;
      velocity = vec3(0.0f);
      setCollidable(false);
      showFace();
      return;
    }
  } else {
    // (otherwise he is a statue: no sliding about from a push, either)
    velocity.x = velocity.z = 0.0f;
    acceleration.x = acceleration.z = 0.0f;
  }
  DynamicGameObject::update(dt);
  animate(dt);
}

// The legs swing with the ground he covers, and the knife arm comes up as he arms; at rest all the
// transforms are the identity: nothing moves
void Gnome::animate(double dt) {
  float dtf = (float)dt;
  vec3 moved = position - lastPosition;
  lastPosition = position;
  float covered = length(vec2(moved.x, moved.z));
  float speed = dt > 0.0 ? covered / dtf : 0.0f;
  walkAmount += (clamp(speed / 2.0f, 0.0f, 1.0f) - walkAmount) * std::min(1.0f, 10.0f * dtf);
  walkPhase = std::fmod(walkPhase + covered / STRIDE * 2.0f * PI, 2.0f * PI);
  raise += clamp((armed ? 1.0f : 0.0f) - raise, -6.0f * dtf, 6.0f * dtf);
  for (int side = 0; side < 2; side++) {
    float phase = walkPhase + (side == 0 ? 0.0f : PI);
    float swing = -LEG_SWING * std::sin(phase) * walkAmount;
    vec3 hip(side == 0 ? HIP.x : -HIP.x, HIP.y, HIP.z);
    setPartTransform(legPart[side], swingAbout(hip, swing));
  }
  // the knife arm: out in front of him, stabbing a little as he runs
  float arm = raise * (ARM_RAISED + 0.18f * std::sin(walkPhase) * walkAmount);
  setPartTransform(armPart, swingAbout(SHOULDER, arm));
  setPartTransform(knifePart, swingAbout(SHOULDER, arm));
}

void Gnome::updateReplica(double dt) {
  GameObject::update(dt);
  showFace();
  animate(dt);
}

void Gnome::writeNetState(NetWriter &out) const {
  out.u8((uint8_t)face);
  out.boolean(armed);
  out.boolean(dead);
}

void Gnome::readNetState(NetReader &in) {
  uint8_t f = in.u8();
  bool a = in.boolean();
  bool d = in.boolean();
  if (!in.isOk() || f >= FACES)
    return;
  face = f;
  armed = a;
  dead = d;
}

void Gnome::describe(vector<string> &lines) const {
  DynamicGameObject::describe(lines);
  lines.push_back("Gnomo: cara " + to_string(face) + "/" + to_string(LAST_FACE) +
                  (dead ? " (muerto)" : armed ? " (con cuchillo, te persigue)" : ""));
}
