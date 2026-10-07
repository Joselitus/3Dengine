#include "Bob.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Saucer.h"
#include "TextFormat.h"

using namespace glm;
using namespace std;

namespace {
const float PI = 3.14159265f;
const float TURN_RATE = 4.0f; // rad/s
// His joints, left side (generate_bob.py: SHOULDER, ELBOW, HIP, KNEE; the right ones mirror them)
const vec3 SHOULDER(0.165f, 1.02f, 0.0f), ELBOW(0.19f, 0.76f, -0.01f);
const vec3 HIP(0.085f, 0.64f, 0.0f), KNEE(0.09f, 0.34f, 0.02f);
// His eyes (generate_bob.py: the middle of each one), where the ray comes from
const vec3 EYE(0.072f, 1.235f, 0.19f);
// How far the limbs swing as he walks (radians, at full stride)
const float HIP_SWING = 0.45f, KNEE_BEND = 0.75f, ARM_SWING = 0.35f, ELBOW_BEND = 0.25f;
const vec3 RAY_COLOR(1.0f, 0.85f, 0.1f);
const float RAY_SHORT = 0.5f; // the ray ends this far (m) from the player's head

float angleTo(float a, float b) {
  float d = std::fmod(b - a + PI, 2.0f * PI);
  if (d < 0.0f)
    d += 2.0f * PI;
  return d - PI;
}

float approach(float value, float target, float step) {
  return value + clamp(target - value, -step, step);
}

// A turn of `angle` about the x axis through `pivot`
mat4 swingAbout(const vec3 &pivot, float angle) {
  mat4 m = glm::translate(mat4(1.0f), pivot);
  m = rotate(m, angle, vec3(1.0f, 0.0f, 0.0f));
  return glm::translate(m, -pivot);
}
} // namespace

Bob::Bob(shared_ptr<Model> body, shared_ptr<Model> eyes, shared_ptr<Model> eyesGlow,
         const vector<shared_ptr<Model>> &limbs, shared_ptr<Model> ray)
    : DynamicGameObject(body, make_shared<Capsule>(0.22f, 1.45f)), random(random_device()()) {
  eyesPart = addPart(eyes);
  glowPart = addPart(eyesGlow, 2); // (white, glowing)
  for (size_t i = 0; i < 8 && i < limbs.size(); i++)
    limbParts[i] = addPart(limbs[i]);
  for (int i = 0; i < 2; i++) {
    rayParts[i] = addPart(ray, 2);
    setPartVisible(rayParts[i], false);
  }
  setGravity(25.0f);
  setMass(50.0f);
  setMaxSpeed(CHASE_SPEED);
  setMaxAcceleration(20.0f);
  setVisible(false);
  setCollidable(false);
}

void Bob::enter(Behavior next) {
  behavior = next;
  stateTime = 0.0f;
  hasGoal = false;
  pause = 0.0f;
}

void Bob::disembark() {
  if (behavior != Behavior::Inside || !ship)
    return;
  tookPlayer = false;
  position = ship->rampTop();
  lastPosition = position;
  velocity = vec3(0.0f);
  fall = reach = 0.0f;
  setVisible(true);
  setCollidable(false); // (on the ramp he goes through the hull)
  setGravity(0.0f);
  enter(Behavior::Exiting);
}

void Bob::teleport(const vec3 &where) {
  DynamicGameObject::teleport(where);
  lastPosition = where;
}

void Bob::struggleOnce() {
  if (behavior == Behavior::Grabbing)
    struggle = std::min(1.0f, struggle + STRUGGLE_PER_PRESS);
}

// He takes the player: the stage abducts him, the ship's beam comes on, and he goes back to it
void Bob::take() {
  tookPlayer = true;
  velocity = vec3(0.0f);
  if (takePlayer)
    takePlayer();
  if (ship)
    ship->takePlayer();
  enter(Behavior::Returning);
}

// Towards a point on the ground, turning to face where he goes
void Bob::walkTo(const vec3 &point, float speed, double dt) {
  vec3 to = point - position;
  to.y = 0.0f;
  float d = length(to);
  vec3 want = d > 0.05f ? to / d * speed * std::min(1.0f, d / 0.6f) : vec3(0.0f);
  steerTowards(vec3(want.x, velocity.y, want.z), 8.0f);
  acceleration.y = 0.0f;
  if (d > 0.1f)
    faceTowards(point, dt);
}

void Bob::faceTowards(const vec3 &point, double dt) {
  vec3 to = point - position;
  if (length(vec2(to.x, to.z)) < 0.01f)
    return;
  float want = std::atan2(to.x, to.z);
  yaw += clamp(angleTo(yaw, want), -TURN_RATE * (float)dt, TURN_RATE * (float)dt);
}

void Bob::update(double dt) {
  float dtf = (float)dt;
  stateTime += dtf;
  if (behavior == Behavior::Inside) {
    GameObject::update(dt);
    return;
  }
  bool night = isNight ? isNight() : true;
  bool inVehicle = playerInVehicle && playerInVehicle();
  bool dead = playerDead && playerDead();
  bool paralysed = playerParalysed && playerParalysed();
  vec3 target = targetPosition ? targetPosition() : position;
  float distance = length(vec2(target.x - position.x, target.z - position.z));
  bool goHome = !night || tookPlayer;
  rayCooldown = std::max(0.0f, rayCooldown - dtf);
  leaveAlone = std::max(0.0f, leaveAlone - dtf);
  bool rays = false; // his ray shines this frame

  switch (behavior) {
  case Behavior::Exiting: {
    // down the ramp, at its pace, then onto the ground
    vec3 top = ship->rampTop(), foot = ship->rampFoot();
    float along = std::min(1.0f, stateTime * RAMP_SPEED / length(foot - top));
    position = mix(top, foot, along);
    faceTowards(foot + (foot - top), dt);
    if (along >= 1.0f) {
      setGravity(25.0f);
      setCollidable(true);
      velocity = vec3(0.0f);
      enter(Behavior::Prowl);
    }
    break;
  }
  case Behavior::Prowl:
    if (goHome) {
      enter(Behavior::Returning);
    } else if (!dead && !inVehicle && distance < CHASE_RANGE && leaveAlone <= 0.0f) {
      enter(Behavior::Chase);
    } else if (pause > 0.0f) {
      pause -= dtf;
      walkTo(position, 0.0f, dt);
      faceTowards(target, dt); // he stares at the player
    } else {
      if (!hasGoal || length(vec2(goal.x - position.x, goal.z - position.z)) < 0.4f) {
        if (hasGoal)
          pause = uniform(2.0f, 4.0f);
        float a = uniform(0.0f, 2.0f * PI);
        float r = PROWL_RADIUS * std::sqrt(uniform(0.1f, 1.0f));
        goal = ship->rampFoot() + vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
        hasGoal = true;
      }
      walkTo(goal, WALK_SPEED, dt);
    }
    break;
  case Behavior::Chase:
    if (goHome) {
      enter(Behavior::Returning);
    } else if (dead || inVehicle || distance > LOSE_RANGE) {
      enter(Behavior::Prowl);
    } else if (distance < CATCH_DISTANCE) {
      if (paralysed) {
        take(); // he can't move: there is no struggle
      } else {
        struggle = 0.0f;
        velocity = vec3(0.0f);
        enter(Behavior::Grabbing);
      }
    } else if (!paralysed && rayCooldown <= 0.0f && distance > RAY_MIN && distance < RAY_MAX &&
               uniform(0.0f, 1.0f) < 1.0f - std::pow(1.0f - RAY_CHANCE, dtf)) {
      rayHit = false;
      velocity = vec3(0.0f);
      enter(Behavior::Firing);
    } else {
      walkTo(target, CHASE_SPEED, dt);
    }
    break;
  case Behavior::Firing:
    // he stops, stares, his eyes flare, and the ray hits the player
    walkTo(position, 0.0f, dt);
    faceTowards(target, dt);
    rays = stateTime >= RAY_CHARGE && !dead && !inVehicle;
    if (rays && !rayHit) {
      rayHit = true;
      if (paralyse)
        paralyse(PARALYSIS_TIME);
    }
    if (stateTime >= RAY_CHARGE + RAY_TIME || goHome || inVehicle || dead) {
      rayCooldown = RAY_COOLDOWN;
      enter(goHome ? Behavior::Returning : Behavior::Chase);
    }
    break;
  case Behavior::Grabbing:
    // he holds him in front of him; each press of the key loosens his grip, which tightens again
    walkTo(position, 0.0f, dt);
    faceTowards(target, dt);
    if (dead || inVehicle) {
      enter(Behavior::Prowl);
    } else if (struggle >= 1.0f) { // (before it wears off this frame)
      // free: Bob falls over on his back
      leaveAlone = LEAVE_ALONE + FALLEN_TIME;
      setCollidable(false);
      velocity = vec3(0.0f);
      enter(Behavior::Fallen);
    } else if (stateTime >= GRAB_TIME) {
      take();
    } else {
      struggle = std::max(0.0f, struggle - STRUGGLE_DECAY * dtf);
    }
    break;
  case Behavior::Fallen:
    walkTo(position, 0.0f, dt);
    if (stateTime >= FALLEN_TIME + 1.0f) {
      setCollidable(true);
      enter(goHome ? Behavior::Returning : Behavior::Prowl);
    }
    break;
  case Behavior::Returning: {
    vec3 foot = ship->rampFoot();
    float d = length(vec2(foot.x - position.x, foot.z - position.z));
    walkTo(foot, tookPlayer ? WALK_SPEED * 1.5f : WALK_SPEED * 1.8f, dt);
    if (d < 0.35f && ship->isRampDown()) { // (he waits at its foot until the ramp is down)
      setGravity(0.0f);
      setCollidable(false);
      velocity = vec3(0.0f);
      enter(Behavior::Boarding);
    }
    break;
  }
  case Behavior::Boarding: {
    vec3 top = ship->rampTop(), foot = ship->rampFoot();
    float along = std::min(1.0f, stateTime * RAMP_SPEED / length(foot - top));
    position = mix(foot, top, along);
    velocity = vec3(0.0f);
    faceTowards(top + (top - foot), dt);
    if (along >= 1.0f) {
      setVisible(false);
      enter(Behavior::Inside);
      ship->boarded(); // the ship closes and goes
    }
    break;
  }
  case Behavior::Inside:
    break;
  }

  if (behavior == Behavior::Exiting || behavior == Behavior::Boarding)
    GameObject::update(dt); // (on the ramp he is moved, not pushed)
  else
    DynamicGameObject::update(dt);
  // falling over on his back (and up again), about his feet
  float fallWanted = behavior == Behavior::Fallen && stateTime < FALLEN_TIME ? 1.0f : 0.0f;
  fall = approach(fall, fallWanted, dtf / (fallWanted > 0.0f ? 0.5f : 1.0f));
  float tip = -1.45f * fall * fall; // (faster as it goes: he topples)
  rotation = rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f)) * rotate(mat4(1.0f), tip, vec3(1.0f, 0.0f, 0.0f));
  reach = approach(reach, behavior == Behavior::Grabbing || behavior == Behavior::Firing ? 1.0f : 0.0f, 4.0f * dtf);
  // his eyes glow at night (and flare while he fires)
  setPartVisible(glowPart, night || behavior == Behavior::Firing);
  setPartVisible(eyesPart, !(night || behavior == Behavior::Firing));
  aimRays(rays);
  animate(dt);
}

// The two rods of the ray, from his eyes to the player's head, in his frame
void Bob::aimRays(bool shining) {
  for (int i = 0; i < 2; i++)
    setPartVisible(rayParts[i], shining);
  if (!shining || !targetPosition)
    return;
  vec3 head = targetPosition() + vec3(0.0f, HEAD_HEIGHT, 0.0f);
  mat3 toBody = transpose(mat3(rotation));
  vec3 aim = toBody * (head - position);
  for (int i = 0; i < 2; i++) {
    vec3 eye(i == 0 ? EYE.x : -EYE.x, EYE.y, EYE.z);
    vec3 d = aim - eye;
    float len = length(d) - RAY_SHORT; // (it stops short of his face: he sees it coming)
    if (len < 0.05f)
      continue;
    d = normalize(d) * len;
    vec3 z = d / len;
    vec3 x = cross(vec3(0.0f, 1.0f, 0.0f), z);
    x = length(x) > 1e-4f ? normalize(x) : vec3(1.0f, 0.0f, 0.0f);
    vec3 y = cross(z, x);
    mat4 m(1.0f);
    m[0] = vec4(x, 0.0f);
    m[1] = vec4(y, 0.0f);
    m[2] = vec4(z * len, 0.0f); // (the rod is 1 m long: stretched to reach his head)
    m[3] = vec4(eye, 1.0f);
    setPartTransform(rayParts[i], m);
  }
}

void Bob::getLight(vector<SpotLight> &lights) const {
  if (behavior != Behavior::Firing || stateTime < RAY_CHARGE)
    return;
  vec3 eyes = position + vec3(rotation * vec4(0.0f, EYE.y, EYE.z + 0.1f, 0.0f));
  lights.push_back(SpotLight::omni(eyes, RAY_COLOR * 1.5f, 8.0f));
}

// The walk: the step follows the ground he covers; hips and shoulders swing in opposition, the
// knee bends on the forward swing, the elbows a little. Reaching for the player, his arms come up
// in front of him.
void Bob::animate(double dt) {
  vec3 moved = position - lastPosition;
  lastPosition = position;
  float covered = length(vec2(moved.x, moved.z));
  float speed = dt > 0.0 ? covered / (float)dt : 0.0f;
  if (behavior == Behavior::Exiting || behavior == Behavior::Boarding)
    speed = RAMP_SPEED;
  walkAmount += (clamp(speed / 1.2f, 0.0f, 1.0f) - walkAmount) * std::min(1.0f, 6.0f * (float)dt);
  walkPhase = std::fmod(walkPhase + std::max(covered, speed * (float)dt) / STRIDE * 2.0f * PI, 2.0f * PI);
  for (int side = 0; side < 2; side++) {
    float s = side == 0 ? 1.0f : -1.0f;  // the right side mirrors the left
    float phase = walkPhase + (side == 0 ? 0.0f : PI);
    float swing = -HIP_SWING * std::sin(phase) * walkAmount; // negative: forward
    float knee = KNEE_BEND * std::max(0.0f, std::cos(phase)) * walkAmount;
    vec3 hip(s * HIP.x, HIP.y, HIP.z), kneeJoint(s * KNEE.x, KNEE.y, KNEE.z);
    vec3 shoulder(s * SHOULDER.x, SHOULDER.y, SHOULDER.z), elbow(s * ELBOW.x, ELBOW.y, ELBOW.z);
    mat4 thigh = swingAbout(hip, swing);
    mat4 shin = thigh * swingAbout(kneeJoint, knee);
    float arm = ARM_SWING * std::sin(phase) * walkAmount; // opposite to the leg of its side
    arm = mix(arm, -1.35f, reach);                         // (reaching: straight out in front)
    mat4 upper = swingAbout(shoulder, arm);
    mat4 fore = upper * swingAbout(elbow, -ELBOW_BEND * (0.4f + 0.6f * walkAmount) * (1.0f - reach));
    setPartTransform(limbParts[side * 4 + 0], upper);
    setPartTransform(limbParts[side * 4 + 1], fore);
    setPartTransform(limbParts[side * 4 + 2], thigh);
    setPartTransform(limbParts[side * 4 + 3], shin);
  }
}

void Bob::describe(vector<string> &lines) const {
  DynamicGameObject::describe(lines);
  const char *names[] = {"en la nave", "bajando la rampa", "merodea", "te persigue",
                         "te dispara el rayo", "te sujeta", "en el suelo",
                         "vuelve a la nave", "sube la rampa"};
  lines.push_back(string("Bob: ") + names[(int)behavior] + (tookPlayer ? " (te lleva)" : ""));
  if (behavior == Behavior::Grabbing)
    lines.push_back(textFormat("Forcejeo: %.0f %%", struggle * 100.0f));
}
