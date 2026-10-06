#ifndef FOLLA_CULOS_RIG
#define FOLLA_CULOS_RIG

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Ragdoll.h"
#include "SpiderGait.h"

// Its skeleton as a ragdoll: the points (the joints of the rig, in the T-pose of the model:
// generate_folla_culos_run.py, BIND and ENDPOINT) and what each bone joins
namespace folla_culos_rig {
enum Point { PELVIS, SPINE1, SPINE2, CHEST, NECK, HEAD, HEAD_TOP, SH_L, SH_R, EL_L, EL_R, WR_L, WR_R,
             TIP_L, TIP_R, HIP_L, HIP_R, KNEE_L, KNEE_R, ANKLE_L, ANKLE_R, TOE_L, TOE_R, POINTS };

inline std::vector<glm::vec3> bindPoints() {
  std::vector<glm::vec3> p(POINTS);
  p[PELVIS] = glm::vec3(0.0f, 1.05f, 0.0f);   p[SPINE1] = glm::vec3(0.0f, 1.25f, 0.0f);
  p[SPINE2] = glm::vec3(0.0f, 1.55f, 0.0f);   p[CHEST] = glm::vec3(0.0f, 1.80f, 0.0f);
  p[NECK] = glm::vec3(0.0f, 1.93f, 0.0f);     p[HEAD] = glm::vec3(0.0f, 2.05f, 0.02f);
  p[HEAD_TOP] = glm::vec3(0.0f, 2.33f, 0.03f);
  for (float s : {1.0f, -1.0f}) {
    int side = s > 0 ? 0 : 1; // L, R
    p[SH_L + side] = glm::vec3(s * 0.255f, 1.84f, 0.0f);
    p[EL_L + side] = glm::vec3(s * 0.82f, 1.84f, -0.02f);
    p[WR_L + side] = glm::vec3(s * 1.38f, 1.84f, 0.0f);
    p[TIP_L + side] = glm::vec3(s * 1.80f, 1.84f, 0.0f);
    p[HIP_L + side] = glm::vec3(s * 0.115f, 1.14f, 0.0f);
    p[KNEE_L + side] = glm::vec3(s * 0.150f, 0.60f, 0.03f);
    p[ANKLE_L + side] = glm::vec3(s * 0.170f, 0.10f, -0.012f);
    p[TOE_L + side] = glm::vec3(s * 0.172f, 0.04f, 0.35f);
  }
  return p;
}

inline std::vector<RagdollBone> bones() {
  std::vector<RagdollBone> b = {
      {"pelvis", PELVIS, SPINE1, HIP_L, HIP_R}, {"spine1", SPINE1, SPINE2, HIP_L, HIP_R},
      {"spine2", SPINE2, CHEST, SH_L, SH_R},    {"chest", CHEST, NECK, SH_L, SH_R},
      {"neck", NECK, HEAD, SH_L, SH_R},         {"head", HEAD, HEAD_TOP, SH_L, SH_R}};
  const char *sides[2] = {"_L", "_R"};
  for (int s = 0; s < 2; s++) {
    std::string n = sides[s];
    b.push_back({"upperarm" + n, SH_L + s, EL_L + s});
    b.push_back({"forearm" + n, EL_L + s, WR_L + s});
    b.push_back({"hand" + n, WR_L + s, TIP_L + s});
    b.push_back({"thigh" + n, HIP_L + s, KNEE_L + s});
    b.push_back({"shin" + n, KNEE_L + s, ANKLE_L + s});
    b.push_back({"foot" + n, ANKLE_L + s, TOE_L + s});
  }
  return b;
}

inline std::vector<RagdollLink> links() {
  std::vector<RagdollLink> l = {
      // the torso is a rigid frame: hips, shoulders and the spine between them
      {PELVIS, HIP_L, 1.0f}, {PELVIS, HIP_R, 1.0f}, {HIP_L, HIP_R, 1.0f},
      {CHEST, SH_L, 1.0f}, {CHEST, SH_R, 1.0f}, {SH_L, SH_R, 1.0f},
      {NECK, SH_L, 1.0f}, {NECK, SH_R, 1.0f}, {SPINE2, SH_L, 1.0f}, {SPINE2, SH_R, 1.0f},
      {SPINE1, HIP_L, 1.0f}, {SPINE1, HIP_R, 1.0f}, {SPINE2, HIP_L, 0.7f}, {SPINE2, HIP_R, 0.7f},
      {PELVIS, CHEST, 0.6f}, {PELVIS, SPINE2, 0.6f}, {SPINE1, CHEST, 0.6f}, {SPINE1, NECK, 0.5f},
      // the head hangs from the neck, not floppy
      {HEAD, CHEST, 0.5f}, {HEAD, SH_L, 0.5f}, {HEAD, SH_R, 0.5f}};
  // arms and legs can't fold right back (a minimum reach, a third of their length)
  l.push_back({SH_L, WR_L, 1.0f, true, 0.3f});
  l.push_back({SH_R, WR_R, 1.0f, true, 0.3f});
  l.push_back({HIP_L, ANKLE_L, 1.0f, true, 0.3f});
  l.push_back({HIP_R, ANKLE_R, 1.0f, true, 0.3f});
  return l;
}

// The bone each point belongs to (to place it from the pose of that bone)
inline const char *boneOf(int point) {
  static const char *names[POINTS] = {"pelvis", "spine1", "spine2", "chest", "neck", "head", "head",
                                      "upperarm_L", "upperarm_R", "forearm_L", "forearm_R", "hand_L",
                                      "hand_R", "hand_L", "hand_R", "thigh_L", "thigh_R", "shin_L",
                                      "shin_R", "foot_L", "foot_R", "foot_L", "foot_R"};
  return names[point];
}
// ...and the joint of that bone (the point it starts at)
inline int jointOf(int point) {
  static const int joints[POINTS] = {PELVIS, SPINE1, SPINE2, CHEST, NECK, HEAD, HEAD, SH_L, SH_R, EL_L,
                                     EL_R, WR_L, WR_R, WR_L, WR_R, HIP_L, HIP_R, KNEE_L, KNEE_R,
                                     ANKLE_L, ANKLE_R, ANKLE_L, ANKLE_R};
  return joints[point];
}

// ...and for walking on four legs (SpiderGait): the spine, what hangs from it, and the legs. The
// arms are the front legs and the legs the hind ones; +x is the left of the model.
inline SpiderRig spiderRig() {
  SpiderRig r;
  r.spine = {PELVIS, SPINE1, SPINE2, CHEST, NECK, HEAD, HEAD_TOP};
  r.neckBone = "neck";
  r.headBone = "head";
  r.neckSpine = 4; // (the neck starts at NECK and the head at HEAD, the 5th and 6th points)
  r.headSpine = 5;
  r.attach = {SH_L, SH_R, HIP_L, HIP_R};
  r.attachTo = {3, 3, 0, 0};
  r.legs.push_back({SH_L, EL_L, WR_L, TIP_L, true, 1.0f});
  r.legs.push_back({SH_R, EL_R, WR_R, TIP_R, true, -1.0f});
  r.legs.push_back({HIP_L, KNEE_L, ANKLE_L, TOE_L, false, 1.0f});
  r.legs.push_back({HIP_R, KNEE_R, ANKLE_R, TOE_R, false, -1.0f});
  return r;
}
} // namespace folla_culos_rig

#endif
