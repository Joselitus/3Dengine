#include "NpcRagdoll.h"


using namespace std;
using namespace glm;

namespace {
// Where a bone's joint is in the bind pose (the inverse of its offset matrix), in the model's units
bool jointOf(const AnimatedModel &model, const string &name, vec3 &joint) {
  mat4 bind;
  if (!model.skeleton.BindGlobal(name, bind))
    return false;
  joint = vec3(bind[3]);
  return true;
}
} // namespace

vec3 NpcRagdoll::toMetres(const vec3 &raw) const { return (raw - model->fitCenter) * model->fitScale; }

NpcRagdoll::NpcRagdoll(shared_ptr<AnimatedModel> model) : model(model) {
  // the bone of each point (a "tip" point is beyond its bone's joint: see below)
  const char *names[POINTS] = {"pelvis",     "spine_01",   "spine_02",   "spine_03",   "spine_04",  "spine_05",
                               "neck_01",    "neck_02",    "neck_02",    "clavicle_l", "clavicle_r",
                               "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "hand_l",    "hand_r",
                               "hand_l",     "hand_r",     "thigh_l",    "thigh_r",    "calf_l",    "calf_r",
                               "foot_l",     "foot_r",     "ball_l",     "ball_r",     "ball_l",    "ball_r"};
  vector<vec3> joints(POINTS);
  for (int i = 0; i < POINTS; i++)
    if (!jointOf(*model, names[i], joints[i]))
      return; // (not this kind of skeleton: invalid)
  bindRaw = joints;
  // the tips: a point beyond the end of a bone, along the way the bone before it points (the head:
  // the middle of the head, beyond the last neck bone)
  // (their lengths are in metres: the model's own units are hundreds of times smaller)
  auto tip = [&](int point, int from, int to, float metres) {
    vec3 d = joints[to] - joints[from];
    bindRaw[point] = joints[to] + d / std::max(glm::length(d), 1e-4f) * (metres / model->fitScale);
  };
  tip(HEAD, NECK1, NECK2, 0.14f);
  tip(HAND_TIP_L, LOWER_L, HAND_L, 0.2f);
  tip(HAND_TIP_R, LOWER_R, HAND_R, 0.2f);
  tip(BALL_TIP_L, FOOT_L, BALL_L, 0.12f);
  tip(BALL_TIP_R, FOOT_R, BALL_R, 0.12f);
  pointBone.assign(names, names + POINTS);
  pointJoint = joints;

  vector<vec3> bind(POINTS);
  for (int i = 0; i < POINTS; i++)
    bind[i] = toMetres(bindRaw[i]);

  vector<RigBone> bones = {
      {"pelvis", PELVIS, SPINE1, THIGH_L, THIGH_R}, {"spine_01", SPINE1, SPINE2, THIGH_L, THIGH_R},
      {"spine_02", SPINE2, SPINE3, CLAV_L, CLAV_R}, {"spine_03", SPINE3, SPINE4, CLAV_L, CLAV_R},
      {"spine_04", SPINE4, SPINE5, CLAV_L, CLAV_R}, {"spine_05", SPINE5, NECK1, CLAV_L, CLAV_R},
      {"neck_01", NECK1, NECK2, CLAV_L, CLAV_R},    {"neck_02", NECK2, HEAD, CLAV_L, CLAV_R}};
  const char *sides[2] = {"_l", "_r"};
  for (int s = 0; s < 2; s++) {
    string n = sides[s];
    // (the arms are flippers, flat blades: their roll follows the spine's direction, so they stay
    // flat to the body and do not stand on edge)
    bones.push_back({"clavicle" + n, CLAV_L + s, UPPER_L + s, SPINE5, PELVIS});
    bones.push_back({"upperarm" + n, UPPER_L + s, LOWER_L + s, SPINE5, PELVIS});
    bones.push_back({"lowerarm" + n, LOWER_L + s, HAND_L + s, SPINE5, PELVIS});
    bones.push_back({"hand" + n, HAND_L + s, HAND_TIP_L + s, SPINE5, PELVIS});
    bones.push_back({"thigh" + n, THIGH_L + s, CALF_L + s});
    bones.push_back({"calf" + n, CALF_L + s, FOOT_L + s});
    bones.push_back({"foot" + n, FOOT_L + s, BALL_L + s});
    bones.push_back({"ball" + n, BALL_L + s, BALL_TIP_L + s});
  }
  vector<RagdollLink> links = {
      // the torso is a rigid frame: the spine, the hips and the shoulders
      {PELVIS, SPINE2, 1.0f}, {SPINE1, SPINE3, 1.0f}, {SPINE2, SPINE4, 1.0f}, {SPINE3, SPINE5, 1.0f},
      {PELVIS, THIGH_L, 1.0f}, {PELVIS, THIGH_R, 1.0f}, {THIGH_L, THIGH_R, 1.0f},
      {SPINE1, THIGH_L, 1.0f}, {SPINE1, THIGH_R, 1.0f},
      {SPINE5, CLAV_L, 1.0f}, {SPINE5, CLAV_R, 1.0f}, {CLAV_L, CLAV_R, 1.0f},
      {SPINE4, CLAV_L, 1.0f}, {SPINE4, CLAV_R, 1.0f}, {NECK1, CLAV_L, 1.0f}, {NECK1, CLAV_R, 1.0f},
      {PELVIS, SPINE5, 0.6f}, {SPINE1, SPINE5, 0.6f},
      // the head hangs from the neck, not floppy
      {NECK2, SPINE5, 0.6f}, {HEAD, SPINE5, 0.5f}, {HEAD, CLAV_L, 0.5f}, {HEAD, CLAV_R, 0.5f}};
  // arms and legs can't fold right back (a minimum reach, a third of their length)
  for (int s = 0; s < 2; s++) {
    links.push_back({UPPER_L + s, HAND_L + s, 1.0f, true, 0.5f});
    links.push_back({THIGH_L + s, FOOT_L + s, 1.0f, true, 0.6f});
  }
  ragdoll.reset(new Ragdoll(bind, bones, links));
  // How thick each part is (the penguin's body is a fat one: its spine must not lie on the floor, or
  // half of it would be inside): the floor holds each point this far up
  vector<float> radii(POINTS, 0.05f);
  for (int i : {PELVIS, SPINE1, SPINE2, SPINE3, SPINE4, SPINE5})
    radii[i] = 0.2f;
  for (int i : {CLAV_L, CLAV_R, THIGH_L, THIGH_R})
    radii[i] = 0.12f;
  radii[NECK1] = radii[NECK2] = 0.09f;
  radii[HEAD] = 0.14f;
  for (int i : {CALF_L, CALF_R, FOOT_L, FOOT_R, BALL_L, BALL_R, BALL_TIP_L, BALL_TIP_R})
    radii[i] = 0.06f;
  ragdoll->setRadii(radii);
  valid = true;
}

void NpcRagdoll::start(const vec3 &position, const mat3 &rotation, const vec3 &velocity) {
  if (!valid)
    return;
  vector<vec3> world(POINTS);
  for (int i = 0; i < POINTS; i++) {
    mat4 bone(1.0f), bind(1.0f);
    vec3 raw = bindRaw[i];
    // the point where the animation has it now: the bone's matrix applied to its place in the bind
    // pose (skinning: pose * offset * point, the offset being the inverse of the bone's bind matrix)
    if (model->skeleton.PoseGlobal(pointBone[i], bone) && model->skeleton.BindGlobal(pointBone[i], bind))
      raw = vec3(bone * inverse(bind) * vec4(bindRaw[i], 1.0f));
    world[i] = position + rotation * toMetres(raw);
  }
  ragdoll->unpin();
  ragdoll->start(world, velocity);
}

void NpcRagdoll::apply(const vec3 &origin) {
  if (!valid)
    return;
  map<string, mat4> pose;
  ragdoll->boneGlobals(origin, pose);
  // the matrices are in metres around the object; the skin works in the model's own units, which
  // the shader then fits (fitCenter, fitScale): the translation goes back to them
  for (auto &entry : pose) {
    vec3 t = vec3(entry.second[3]) / model->fitScale + model->fitCenter;
    // (a bone's own axes in the bind pose are not the world's: what turns it from there is the
    // rotation the points give, after the bind one, so that the bind pose itself skins to itself)
    mat4 bind(1.0f);
    mat3 linear = mat3(entry.second);
    if (model->skeleton.BindGlobal(entry.first, bind))
      linear = linear * mat3(bind);
    entry.second = mat4(linear);
    entry.second[3] = vec4(t, 1.0f);
  }
  model->setBoneGlobals(pose, "pelvis"); // (the root and the helper bones go with the pelvis)
}
