#include "DeathPose.h"

#include <cmath>
#include <map>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace glm;

// Measured on PenguinoAnimado.fbx's bind pose (see SeatedPose.cpp): metres, feet at the origin, facing +z
static const vec3 HIP(0.0f, 0.968f, -0.089f), KNEE_Y_Z(0.0f, 0.546f, -0.091f);
static const vec3 NECK(0.0f, 1.1f, 0.0f);
static const char *THIGH_BONE[2] = {"thigh_l", "thigh_r"};
static const char *CALF_BONE[2] = {"calf_l", "calf_r"};
static const char *TOE_BONE[2] = {"ball_l", "ball_r"};
static const char *FLIPPER_BONE[2] = {"lowerarm_l", "lowerarm_r"};
static const vec3 FLIPPER_ROOT[2] = {vec3(0.16f, 1.0f, -0.07f), vec3(-0.16f, 1.0f, -0.07f)};
static const vec3 FLIPPER_BIND[2] = {vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f)};

// The fall: a rod hinged at the heels (the client's death camera has the same numbers)
static const float G = 9.81f, START_ANGLE = 0.04f, START_SPEED = 0.3f, BOUNCE = 0.3f;
// Where the heels are (the pivot), and how thick the body is: lying on its back the axis is this high
static const vec3 HEEL(0.0f, 0.0f, -0.1f);
static const float BODY_RADIUS = 0.42f;
// The flippers flail while it falls (elevation above the horizontal, radians), then lie thrown wide
static const float FLAIL_RATE = 11.0f, FLAIL_UP = 0.9f, FLAIL_SWING = 0.5f, SPREAD_UP = 0.25f;
// Lying: the thighs are kicked up (the lower half of the body goes towards the belly), splayed apart,
// the knees bent; the head drops back
static const float LEGS_UP = 0.55f, LEGS_SPLAY = 0.38f, KNEE_BEND = 0.5f, HEAD_BACK = 0.5f, TOES_UP = 0.5f;

// A turn about an axis through `pivot` (positive about x: what is above goes forward, +z)
static mat4 hinge(const vec3 &pivot, float angle, const vec3 &axis = vec3(1.0f, 0.0f, 0.0f)) {
  mat4 m = translate(mat4(1.0f), pivot);
  m = rotate(m, angle, axis);
  return translate(m, -pivot);
}

DeathPose::DeathPose(std::shared_ptr<AnimatedModel> model) : model(model) {
  mat4 bind;
  for (int s = 0; s < 2; s++)
    if (!model->skeleton.BindGlobal(FLIPPER_BONE[s], bind) || !model->skeleton.BindGlobal(TOE_BONE[s], bind) ||
        !model->skeleton.BindGlobal(THIGH_BONE[s], bind) || !model->skeleton.BindGlobal(CALF_BONE[s], bind))
      return;
  if (!model->skeleton.BindGlobal("pelvis", bind))
    return;
  // (the measurements are with the model fitted to its bind pose: the idle pose fits it that way)
  if (!model->isIdle()) {
    model->setIdle(true);
    model->setIdle(false);
  }
  restart();
  valid = true;
}

void DeathPose::restart() {
  time = 0.0;
  angle = START_ANGLE;
  speed = START_SPEED;
  landed = -1.0;
}

void DeathPose::step(double dt) {
  time += dt;
  speed += 1.5f * G / EYE_HEIGHT * std::sin(angle) * (float)dt;
  angle += speed * (float)dt;
  if (angle >= HALF_TURN) { // it hits the floor: a small bounce, and it settles
    angle = HALF_TURN;
    speed = speed > 0.0f ? -speed * BOUNCE : 0.0f;
    if (landed < 0.0)
      landed = time;
  }
}

mat4 DeathPose::toModelUnits(const mat4 &metres) const {
  mat4 toMetres = scale(mat4(1.0f), vec3(model->fitScale)) * translate(mat4(1.0f), -model->fitCenter);
  mat4 toRaw = translate(mat4(1.0f), model->fitCenter) * scale(mat4(1.0f), vec3(1.0f / model->fitScale));
  return toRaw * metres * toMetres;
}

void DeathPose::apply() {
  if (!valid)
    return;
  std::map<std::string, mat4> pose;
  auto put = [&](const std::string &bone, const mat4 &metres) {
    mat4 bind;
    if (model->skeleton.BindGlobal(bone, bind))
      pose[bone] = toModelUnits(metres) * bind;
  };
  float w = angle / HALF_TURN; // 0 upright .. 1 lying
  float sinceLanding = landed < 0.0 ? 0.0f : (float)(time - landed);
  // the whole body tips about the heels (backwards: -x), and rests on its back once it is lying
  mat4 body = translate(mat4(1.0f), vec3(0.0f, BODY_RADIUS * std::sin(angle), 0.0f)) * hinge(HEEL, -angle);
  mat3 axes = mat3(body);
  put("pelvis", body);

  // the legs: kicked up and apart as it goes over, with a last twitch of the right one when it lands
  float twitch = 0.12f * std::exp(-2.0f * sinceLanding) * std::sin(14.0f * sinceLanding);
  for (int s = 0; s < 2; s++) {
    float side = s == 0 ? 1.0f : -1.0f;
    mat4 thigh = body * hinge(HIP, side * LEGS_SPLAY * w, vec3(0.0f, 0.0f, 1.0f)) *
                 hinge(HIP, -(LEGS_UP * w + (s == 1 ? twitch : 0.0f)));
    mat4 calf = thigh * hinge(KNEE_Y_Z, KNEE_BEND * w);
    put(THIGH_BONE[s], thigh);
    put(CALF_BONE[s], calf);
    mat4 bind;
    model->skeleton.BindGlobal(TOE_BONE[s], bind);
    vec3 ball = (vec3(bind[3]) - model->fitCenter) * model->fitScale;
    put(TOE_BONE[s], calf * rotate(translate(mat4(1.0f), ball), -TOES_UP * w, vec3(1.0f, 0.0f, 0.0f)) *
                         translate(mat4(1.0f), -ball));
    put(s == 0 ? "ik_foot_l" : "ik_foot_r", calf);

    // the flipper: flailing up over its head while it falls, thrown out wide at the end (the left
    // one a little higher than the right: it is never symmetrical)
    float flail = std::sin(FLAIL_RATE * (float)time + (s == 0 ? 0.0f : 2.3f)) * FLAIL_SWING;
    float elevation = glm::mix(FLAIL_UP + flail, SPREAD_UP + (s == 0 ? 0.18f : -0.05f), w * w);
    vec3 direction = normalize(vec3(side * std::cos(elevation), std::sin(elevation), 0.12f * w));
    vec3 root = vec3(body * vec4(FLIPPER_ROOT[s], 1.0f));
    mat4 turn = mat4_cast(rotation(normalize(axes * FLIPPER_BIND[s]), normalize(axes * direction)));
    mat4 flipper = translate(mat4(1.0f), root) * turn * mat4(axes) * translate(mat4(1.0f), -FLIPPER_ROOT[s]);
    put(FLIPPER_BONE[s], flipper);
    put(s == 0 ? "ik_hand_l" : "ik_hand_r", flipper);
    if (s == 1)
      put("ik_hand_gun", flipper);
  }
  // the head drops back, the beak open to the sky
  put("head", body * hinge(NECK, -HEAD_BACK * w));
  model->setIdle(false);
  model->setBoneGlobals(pose, "pelvis");
}
