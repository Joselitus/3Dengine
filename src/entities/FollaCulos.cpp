#include "FollaCulos.h"

#include <cmath>

using namespace std;
using namespace glm;

// Its skeleton as a ragdoll: the points (the joints of the rig, in the T-pose of the model:
// generate_folla_culos_run.py, BIND and ENDPOINT) and what each bone joins
namespace {
enum Point { PELVIS, SPINE1, SPINE2, CHEST, NECK, HEAD, HEAD_TOP, SH_L, SH_R, EL_L, EL_R, WR_L, WR_R,
             TIP_L, TIP_R, HIP_L, HIP_R, KNEE_L, KNEE_R, ANKLE_L, ANKLE_R, TOE_L, TOE_R, POINTS };

vector<vec3> bindPoints() {
  vector<vec3> p(POINTS);
  p[PELVIS] = vec3(0.0f, 1.05f, 0.0f);   p[SPINE1] = vec3(0.0f, 1.25f, 0.0f);
  p[SPINE2] = vec3(0.0f, 1.55f, 0.0f);   p[CHEST] = vec3(0.0f, 1.80f, 0.0f);
  p[NECK] = vec3(0.0f, 1.93f, 0.0f);     p[HEAD] = vec3(0.0f, 2.05f, 0.02f);
  p[HEAD_TOP] = vec3(0.0f, 2.33f, 0.03f);
  for (float s : {1.0f, -1.0f}) {
    int side = s > 0 ? 0 : 1; // L, R
    p[SH_L + side] = vec3(s * 0.255f, 1.84f, 0.0f);
    p[EL_L + side] = vec3(s * 0.82f, 1.84f, -0.02f);
    p[WR_L + side] = vec3(s * 1.38f, 1.84f, 0.0f);
    p[TIP_L + side] = vec3(s * 1.80f, 1.84f, 0.0f);
    p[HIP_L + side] = vec3(s * 0.115f, 1.14f, 0.0f);
    p[KNEE_L + side] = vec3(s * 0.150f, 0.60f, 0.03f);
    p[ANKLE_L + side] = vec3(s * 0.170f, 0.10f, -0.012f);
    p[TOE_L + side] = vec3(s * 0.172f, 0.04f, 0.35f);
  }
  return p;
}

vector<RagdollBone> bones() {
  vector<RagdollBone> b = {
      {"pelvis", PELVIS, SPINE1, HIP_L, HIP_R}, {"spine1", SPINE1, SPINE2, HIP_L, HIP_R},
      {"spine2", SPINE2, CHEST, SH_L, SH_R},    {"chest", CHEST, NECK, SH_L, SH_R},
      {"neck", NECK, HEAD, SH_L, SH_R},         {"head", HEAD, HEAD_TOP, SH_L, SH_R}};
  const char *sides[2] = {"_L", "_R"};
  for (int s = 0; s < 2; s++) {
    string n = sides[s];
    b.push_back({"upperarm" + n, SH_L + s, EL_L + s});
    b.push_back({"forearm" + n, EL_L + s, WR_L + s});
    b.push_back({"hand" + n, WR_L + s, TIP_L + s});
    b.push_back({"thigh" + n, HIP_L + s, KNEE_L + s});
    b.push_back({"shin" + n, KNEE_L + s, ANKLE_L + s});
    b.push_back({"foot" + n, ANKLE_L + s, TOE_L + s});
  }
  return b;
}

vector<RagdollLink> links() {
  vector<RagdollLink> l = {
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
const char *boneOf(int point) {
  static const char *names[POINTS] = {"pelvis", "spine1", "spine2", "chest", "neck", "head", "head",
                                      "upperarm_L", "upperarm_R", "forearm_L", "forearm_R", "hand_L",
                                      "hand_R", "hand_L", "hand_R", "thigh_L", "thigh_R", "shin_L",
                                      "shin_R", "foot_L", "foot_R", "foot_L", "foot_R"};
  return names[point];
}
// ...and the joint of that bone (the point it starts at)
int jointOf(int point) {
  static const int joints[POINTS] = {PELVIS, SPINE1, SPINE2, CHEST, NECK, HEAD, HEAD, SH_L, SH_R, EL_L,
                                     EL_R, WR_L, WR_R, WR_L, WR_R, HIP_L, HIP_R, KNEE_L, KNEE_R,
                                     ANKLE_L, ANKLE_R, ANKLE_L, ANKLE_R};
  return joints[point];
}
} // namespace

FollaCulos::FollaCulos(shared_ptr<AnimatedModel> running, shared_ptr<AnimatedModel> splat,
                       SoundEngine &engine, SpeechSynthesizer &synthesizer)
    : Npc(running, "Folla Culos", vector<string>(), engine, synthesizer),
      ragdoll(bindPoints(), bones(), links()) {
  running->useRealSize();
  splat->useRealSize();
  addMesh(splat); // mesh 1
  setDrag(0.0f);               // it runs at its own pace (no NPC drag)
  setMaxSpeed(RUN_SPEED);
  setMaxAcceleration(60.0f);
}

void FollaCulos::kill() {
  if (dead || stuck || ragdolling)
    return;
  running = false;
  velocity = acceleration = vec3(0.0f);
  setCollidable(false);   // nothing bumps into the body that is not there
  if (frontHit && surfaceFrame && frontHit(*this)) {
    // Run over from the front: it ends up spread out on the windshield, for good
    criticalCondition = true;
    stuck = true;
    setMesh(Splat);
  } else {
    dead = true;
    setVisible(false);    // not drawn (and with it, its light: see getLight)
  }
}

// It lets go of the windshield and falls: from where its bones are now (as the splat animation
// has them) it becomes a ragdoll, moving as the vehicle was
void FollaCulos::startRagdoll() {
  if (!stuck || !aniModel)
    return;
  vector<vec3> world(POINTS);
  vector<vec3> bind = bindPoints();
  mat3 axes(rotation);
  for (int i = 0; i < POINTS; i++) {
    mat4 bone;
    vec3 local = bind[i];
    if (aniModel->getBoneGlobal(boneOf(i), bone))
      local = vec3(bone * vec4(bind[i] - bind[jointOf(i)], 1.0f));
    world[i] = position + axes * local;
  }
  criticalCondition = false;
  stuck = false;
  dead = true;
  ragdolling = true;
  setMesh(Running); // the model whose bones the ragdoll poses
  rotation = mat4(1.0f);
  vec3 velocity = carrierVelocity ? carrierVelocity() : vec3(0.0f);
  ragdoll.start(world, velocity);
}

void FollaCulos::applyCollision(const vec3 &push, const vec3 &velocityChange) {
  if (dead || stuck)
    return;
  Npc::applyCollision(push, velocityChange);
  if (length(vec2(velocityChange.x, velocityChange.z)) > DEATH_SPEED_CHANGE)
    kill();
}

void FollaCulos::update(double dt) {
  if (dead || stuck) {
    if (stuck && surfaceFrame) {
      // It follows the windshield: its chest on the glass, facing it (its front, +z, goes into
      // the glass) and its head up the slope
      vec3 center, up, normal;
      surfaceFrame(center, up, normal);
      vec3 into = -normal;
      mat3 axes(cross(up, into), up, into);
      position = center + normal * STUCK_OFFSET - axes * vec3(0.0f, CHEST_HEIGHT, 0.0f);
      rotation = mat4(axes);
      splatClock += dt;
      if (aniModel)
        aniModel->Update(splatClock);
      // The vehicle slows down: it lets go
      if (carrierVelocity) {
        vec3 v = carrierVelocity();
        if (length(vec2(v.x, v.z)) < RAGDOLL_SPEED)
          startRagdoll();
      }
    }
    if (ragdolling) {
      ragdoll.step(dt, floorHeight, pushOut);
      position = ragdoll.getPoints()[PELVIS];
      map<string, mat4> pose;
      ragdoll.boneGlobals(position, pose);
      if (aniModel)
        aniModel->setBoneGlobals(pose);
    }
    return; // (otherwise it stays where it died)
  }
  // A straight line towards the target, on the ground plane
  vec3 wanted(0.0f);
  running = false;
  if (targetPosition) {
    vec3 d = targetPosition() - position;
    d.y = 0.0f;
    float distance = length(d);
    bool night = !isNight || isNight();
    // At night it goes for the target; by day it goes away from it
    bool goes = night ? distance > STOP_DISTANCE : distance < FLEE_DISTANCE;
    if (goes && distance > 1e-3f) {
      vec3 direction = night ? d / distance : -d / distance;
      wanted = direction * RUN_SPEED;
      faceTowards(position + direction); // yaw 0 is towards +z, like the Walker
      running = true;
    }
  }
  wanted.y = velocity.y; // (falling is not steered)
  steerTowards(wanted, 12.0f);
  DynamicGameObject::update(dt);
  // The animation: only while it runs (it stands still in the pose it stopped in)
  if (running)
    runClock += dt;
  if (aniModel)
    aniModel->Update(runClock);
}

void FollaCulos::getLight(vector<SpotLight> &lights) const {
  if (!visible)
    return;
  vec3 forward = vec3(rotation * vec4(0.0f, 0.0f, 1.0f, 0.0f));
  if (ragdolling) // (the head is wherever the ragdoll has it)
    lights.push_back(SpotLight::omni(ragdoll.getPoints()[HEAD], vec3(0.55f, 0.42f, 0.02f),
                                     EYE_LIGHT_RANGE));
  else if (!stuck)
  lights.push_back(SpotLight::omni(position + vec3(0.0f, EYE_HEIGHT, 0.0f) + forward * 0.5f,
                                   vec3(0.55f, 0.42f, 0.02f), EYE_LIGHT_RANGE));
}

void FollaCulos::describe(vector<string> &lines) const {
  Npc::describe(lines);
  lines.push_back(string("Muerto: ") + (!dead ? "no" : ragdolling ? "SI (ragdoll)" : "SI (no se dibuja)"));
  if (stuck)
    lines.push_back("Pegado al parabrisas, agonizando");
  lines.push_back(string("Estado critico: ") + (criticalCondition ? "SI" : "no"));
  lines.push_back(string("Corriendo: ") + (running ? "si" : "no") +
                  ((!isNight || isNight()) ? "  (de noche: hacia el jugador)" : "  (de dia: huye)"));
}
