#include "SeatedPose.h"

#include <cmath>
#include <map>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace glm;

// Measured on PenguinoAnimado.fbx's bind pose, in metres with its feet at the origin (facing +z, left
// on +x): its body is an egg 1.4 m tall, the beak at 1.3 m; the flippers stick out sideways (T pose)
// at 1 m from their roots by the body, 0.78 m long, weighted to the lower arm bones and below
static const vec3 HIP(0.0f, 0.968f, -0.089f), KNEE_Y_Z(0.0f, 0.546f, -0.091f); // (the joints, without x)
static const char *THIGH_BONE[2] = {"thigh_l", "thigh_r"};
static const char *CALF_BONE[2] = {"calf_l", "calf_r"};
static const vec3 FLIPPER_ROOT[2] = {vec3(0.16f, 1.0f, -0.07f), vec3(-0.16f, 1.0f, -0.07f)};
static const vec3 FLIPPER_BIND[2] = {vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f)};
static const char *FLIPPER_BONE[2] = {"lowerarm_l", "lowerarm_r"};
static const char *TOE_BONE[2] = {"ball_l", "ball_r"};
static const vec3 BEAK(-0.02f, 1.3f, 0.36f); // in front of the beak, a little to his right
// Sitting: the thighs (the lower half of the body) go forward, the shins hang from the knees; the
// hips this far over the cushion
static const float THIGH_FOLD = 1.3f, KNEE_BEND = 1.3f, HIP_OVER_SEAT = 0.18f;
// How far forward the body leans about the hips (radians: the driver, to reach the wheel; the
// passenger, back), and how much it breathes
static const float DRIVER_LEAN = 0.15f, PASSENGER_LEAN = -0.08f;
static const float BREATH_LEAN = 0.012f, BREATH_RATE = 0.25f; // radians, breaths a second
static const float TOES_UP = 0.45f;                             // radians
// Where a flipper rests on the lap (a direction from its root, in the leaning body's frame)
static const vec3 FLIPPER_REST[2] = {vec3(0.25f, -0.15f, 0.95f), vec3(-0.25f, -0.15f, 0.95f)};
// The can: how far along the right flipper it stands, and how far over the blade its centre is
static const float CAN_ALONG = 0.45f, CAN_OVER = 0.05f;
// A sip: the can goes up in SIP_RAISE seconds, stays at the beak SIP_HOLD, comes down in SIP_RAISE;
// the next comes SIP_PAUSE_MIN..MAX seconds later
static const float SIP_RAISE = 0.8f, SIP_HOLD = 1.6f, SIP_PAUSE_MIN = 8.0f, SIP_PAUSE_MAX = 16.0f;

SeatedPose::SeatedPose(std::shared_ptr<AnimatedModel> model) : model(model), random(std::random_device()()) {
  mat4 bind;
  for (int s = 0; s < 2; s++)
    if (!model->skeleton.BindGlobal(FLIPPER_BONE[s], bind) || !model->skeleton.BindGlobal(TOE_BONE[s], bind))
      return;
  if (!model->skeleton.BindGlobal("pelvis", bind) || !model->skeleton.BindGlobal(THIGH_BONE[0], bind) ||
      !model->skeleton.BindGlobal(THIGH_BONE[1], bind) || !model->skeleton.BindGlobal(CALF_BONE[0], bind) ||
      !model->skeleton.BindGlobal(CALF_BONE[1], bind))
    return;
  // The measurements above are with the model fitted to its bind pose (feet at the origin): the
  // idle pose fits it that way (and the walker keeps that fit afterwards anyway)
  if (!model->isIdle()) {
    model->setIdle(true);
    model->setIdle(false);
  }
  nextSip = std::uniform_real_distribution<double>(2.0, SIP_PAUSE_MAX)(random);
  valid = true;
}

void SeatedPose::step(double dt) {
  time += dt;
  if (driver && time >= nextSip) {
    sipStart = time;
    nextSip = time + 2.0 * SIP_RAISE + SIP_HOLD +
              std::uniform_real_distribution<double>(SIP_PAUSE_MIN, SIP_PAUSE_MAX)(random);
  }
}

float SeatedPose::lean() const {
  return (driver ? DRIVER_LEAN : PASSENGER_LEAN) + BREATH_LEAN * (float)std::sin(time * 6.2831853 * BREATH_RATE);
}

float SeatedPose::sipAmount() const {
  if (!driver)
    return 0.0f;
  float t = (float)(time - sipStart);
  float up = t < SIP_RAISE + SIP_HOLD ? t / SIP_RAISE : (2.0f * SIP_RAISE + SIP_HOLD - t) / SIP_RAISE;
  up = clamp(up, 0.0f, 1.0f);
  return up * up * (3.0f - 2.0f * up);
}

vec3 SeatedPose::originFromSeat() { return vec3(0.0f, HIP_OVER_SEAT - HIP.y, -HIP.z); }

// A turn about an axis along x through `pivot` (positive: what is above it goes forward, +z)
static mat4 hinge(const vec3 &pivot, float angle) {
  mat4 m = translate(mat4(1.0f), pivot);
  m = rotate(m, angle, vec3(1.0f, 0.0f, 0.0f));
  return translate(m, -pivot);
}

mat4 SeatedPose::bodyTransform() const { return hinge(HIP, lean()); }

vec3 SeatedPose::flipperRoot(int side) const { return vec3(bodyTransform() * vec4(FLIPPER_ROOT[side], 1.0f)); }

vec3 SeatedPose::flipperDirection(int side) const {
  mat3 body = mat3(bodyTransform());
  vec3 root = flipperRoot(side);
  if (driver && side == 0)
    return normalize(grip - root);
  vec3 rest = body * normalize(FLIPPER_REST[side]);
  if (!driver || side == 0)
    return rest;
  vec3 beak = vec3(bodyTransform() * vec4(BEAK, 1.0f));
  return normalize(mix(rest, normalize(beak - root), sipAmount()));
}

mat4 SeatedPose::toModelUnits(const mat4 &metres) const {
  // metres = (raw - fitCenter) * fitScale
  mat4 toMetres = scale(mat4(1.0f), vec3(model->fitScale)) * translate(mat4(1.0f), -model->fitCenter);
  mat4 toRaw = translate(mat4(1.0f), model->fitCenter) * scale(mat4(1.0f), vec3(1.0f / model->fitScale));
  return toRaw * metres * toMetres;
}

void SeatedPose::apply() {
  if (!valid)
    return;
  // Each bone named here is given its whole transform (in metres, applied to the bind pose); the rest
  // follow the nearest of them they hang from (Skeleton::SetPose)
  std::map<std::string, mat4> pose;
  auto put = [&](const std::string &bone, const mat4 &metres) {
    mat4 bind;
    if (model->skeleton.BindGlobal(bone, bind))
      pose[bone] = toModelUnits(metres) * bind;
  };
  mat4 body = bodyTransform();
  put("pelvis", body);
  // the legs (both hips, and both knees, are on one line along x: one turn does for both)
  mat4 thigh = hinge(HIP, -THIGH_FOLD), calf = thigh * hinge(KNEE_Y_Z, KNEE_BEND);
  for (int s = 0; s < 2; s++) {
    put(THIGH_BONE[s], thigh);
    put(CALF_BONE[s], calf);
    // the flipper: from its leaning bind direction to where it points, about its root
    vec3 root = flipperRoot(s);
    mat4 turn = mat4_cast(rotation(normalize(mat3(body) * FLIPPER_BIND[s]), flipperDirection(s)));
    mat4 flipper = translate(mat4(1.0f), root) * turn * mat4(mat3(body)) * translate(mat4(1.0f), -FLIPPER_ROOT[s]);
    put(FLIPPER_BONE[s], flipper);
    // (the mesh is weighted to the rig's IK bones too: they go with what they belong to)
    put(s == 0 ? "ik_hand_l" : "ik_hand_r", flipper);
    if (s == 1)
      put("ik_hand_gun", flipper);
    // the toes, up (about the ball of the foot)
    mat4 bind;
    model->skeleton.BindGlobal(TOE_BONE[s], bind);
    vec3 ball = (vec3(bind[3]) - model->fitCenter) * model->fitScale;
    mat4 toes = translate(mat4(1.0f), ball);
    toes = rotate(toes, -TOES_UP, vec3(1.0f, 0.0f, 0.0f));
    put(TOE_BONE[s], calf * translate(toes, -ball));
    put(s == 0 ? "ik_foot_l" : "ik_foot_r", calf);
  }
  model->setIdle(false);
  model->setBoneGlobals(pose, "pelvis"); // (the root and the helper bones go with the body)
}

mat4 SeatedPose::canTransform() const {
  mat3 body = mat3(bodyTransform());
  vec3 root = flipperRoot(1), along = flipperDirection(1);
  vec3 where = root + along * CAN_ALONG + body * vec3(0.0f, CAN_OVER, 0.0f);
  // upright on the lap; at the beak, its top to the beak and tipped up to pour
  vec3 beak = vec3(bodyTransform() * vec4(BEAK, 1.0f));
  vec3 toBeak = beak - where;
  vec3 pour = normalize(toBeak - vec3(0.0f, 0.6f * length(toBeak), 0.0f) + body * vec3(0.0f, 0.0f, -0.05f));
  vec3 axis = normalize(mix(body * vec3(0.0f, 1.0f, 0.0f), pour, sipAmount()));
  mat4 m = mat4_cast(rotation(vec3(0.0f, 1.0f, 0.0f), axis));
  m[3] = vec4(where, 1.0f);
  return m;
}
