// The folla_culos creature (assets/folla_culos) for the creature testing tool: its procedural
// walk (SpiderGait), its ragdoll, and the two animations recorded in its .glb for comparison.
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "AnimatedModel.h"
#include "FollaCulosRig.h"
#include "GameObject.h"
#include "Ragdoll.h"
#include "SpiderGait.h"
#include "Stage.h"
#include "TestAnimation.h"

using namespace std;
using namespace glm;
using namespace folla_culos_rig;

namespace {

const char *MODEL_FILE = "../assets/folla_culos/folla_culos_run.glb";
const float RUN_SPEED = 4.34f; // FollaCulos::RUN_SPEED

mat3 yawAxes(float heading) { return mat3(rotate(mat4(1.0f), heading, vec3(0.0f, 1.0f, 0.0f))); }

// What they share: the creature's model, and its object in the stage
class FollaAnimation : public TestAnimation {
protected:
  Stage &stage;
  shared_ptr<AnimatedModel> model;
  shared_ptr<GameObject> object;

  FollaAnimation(Stage &stage, unsigned int recordedAnimation, bool feetAtOrigin) : stage(stage) {
    model = make_shared<AnimatedModel>(MODEL_FILE, feetAtOrigin, recordedAnimation);
    model->useRealSize(); // (it is already in metres, feet at y = 0)
    object = make_shared<GameObject>(model);
    object->setCollidable(false);
    stage.add(object);
  }
  void place(const TestBody &body) {
    object->setPosition(body.position.x, body.position.y, body.position.z);
    object->setYaw(body.heading);
  }
};

// The procedural walk: four legs that step (SpiderGait)
class WalkAnimation : public FollaAnimation {
  SpiderGait gait;

public:
  explicit WalkAnimation(Stage &stage)
      : FollaAnimation(stage, 0, true), gait(bindPoints(), bones(), spiderRig()) {
    gait.setFloor([&stage](float x, float z, float &height) { return stage.floorAt(x, z, height); });
  }
  string name() const override { return "walk (procedural, SpiderGait)"; }
  void reset(const TestBody &) override { gait.reset(); }
  void update(double dt, TestBody &body) override {
    place(body);
    gait.setBody(body.position, yawAxes(body.heading), body.velocity);
    gait.setLookTarget(body.viewer); // it looks at the camera
    gait.step(dt);
    map<string, mat4> pose;
    gait.boneGlobals(vec3(0.0f), pose);
    model->setBoneGlobals(pose);
  }
  void joints(vector<vec3> &out) const override {
    mat3 axes = yawAxes(object->getHeading());
    for (const vec3 &p : gait.getPoints())
      out.push_back(object->getPosition() + axes * p);
  }
  string status() const override {
    int inAir = 0;
    for (size_t i = 0; i < gait.legCount(); i++)
      inAir += gait.isSwinging(i);
    return "feet in the air: " + to_string(inAir);
  }
};

// An animation recorded in the file (0 = run, in place; 1 = splat)
class RecordedAnimation : public FollaAnimation {
  string label;

public:
  RecordedAnimation(Stage &stage, unsigned int index, const string &label)
      : FollaAnimation(stage, index, index == 0), label(label) {}
  string name() const override { return label; }
  void reset(const TestBody &) override {}
  void update(double, TestBody &body) override { place(body); } // (the object plays it itself)
  // Its joints where the recorded pose has them
  void joints(vector<vec3> &out) const override {
    vector<vec3> bind = bindPoints();
    mat3 axes = yawAxes(object->getHeading());
    for (int i = 0; i < POINTS; i++) {
      mat4 bone;
      vec3 local = bind[i];
      if (model->getBoneGlobal(boneOf(i), bone))
        local = vec3(bone * vec4(bind[i] - bind[jointOf(i)], 1.0f));
      out.push_back(object->getPosition() + axes * local);
    }
  }
};

// The ragdoll: it is dropped standing and falls; held by the mouse it is carried up, and it
// is thrown when it is let go
class RagdollAnimation : public FollaAnimation {
  Ragdoll ragdoll;
  Ragdoll::FloorQuery floor;
  static constexpr float DROP_HEIGHT = 1.5f; // how far above the floor it starts
  static constexpr float HOLD_HEIGHT = 1.8f; // where the pelvis is while it is held

  void start(const vec3 &pelvisAt, float heading, const vec3 &velocity) {
    vector<vec3> bind = bindPoints(), world(POINTS);
    mat3 axes = yawAxes(heading);
    for (int i = 0; i < POINTS; i++)
      world[i] = pelvisAt + axes * (bind[i] - bind[PELVIS]);
    ragdoll.start(world, velocity);
  }

public:
  explicit RagdollAnimation(Stage &stage)
      : FollaAnimation(stage, 0, true), ragdoll(bindPoints(), bones(), links()) {
    floor = [&stage](float x, float z, float &height) { return stage.floorAt(x, z, height); };
    ragdoll.setWorld(floor, nullptr);
  }
  string name() const override { return "ragdoll"; }
  void reset(const TestBody &body) override {
    start(body.position + vec3(0.0f, bindPoints()[PELVIS].y + DROP_HEIGHT, 0.0f), body.heading,
          vec3(0.0f));
  }
  void update(double dt, TestBody &body) override {
    if (body.held) // carried: all of it goes where the mouse takes it
      start(body.position + vec3(0.0f, HOLD_HEIGHT, 0.0f), body.heading, body.velocity);
    else
      ragdoll.step(dt);
    vec3 pelvis = ragdoll.getPoints()[PELVIS];
    float ground = 0.0f;
    stage.floorAt(pelvis.x, pelvis.z, ground);
    body.position = vec3(pelvis.x, ground, pelvis.z); // (the tool follows where it goes)
    object->setPosition(pelvis.x, pelvis.y, pelvis.z);
    object->setRotation(mat4(1.0f));
    map<string, mat4> pose;
    ragdoll.boneGlobals(pelvis, pose);
    model->setBoneGlobals(pose);
  }
  void joints(vector<vec3> &out) const override {
    for (const vec3 &p : ragdoll.getPoints())
      out.push_back(p);
  }
  bool walks() const override { return false; }
  string status() const override { return ragdoll.isAsleep() ? "asleep" : "moving"; }
};

} // namespace

CreatureEntry follaCulosEntry() {
  CreatureEntry folla;
  folla.name = "folla_culos";
  folla.animations = {"walk (procedural, SpiderGait)", "ragdoll", "run (recorded, in place)",
                      "splat (recorded)"};
  folla.walkSpeed = RUN_SPEED;
  folla.create = [](int index, Stage &stage) -> unique_ptr<TestAnimation> {
    switch (index) {
    case 0:
      return unique_ptr<TestAnimation>(new WalkAnimation(stage));
    case 1:
      return unique_ptr<TestAnimation>(new RagdollAnimation(stage));
    case 2:
      return unique_ptr<TestAnimation>(new RecordedAnimation(stage, 0, "run (recorded, in place)"));
    default:
      return unique_ptr<TestAnimation>(new RecordedAnimation(stage, 1, "splat (recorded)"));
    }
  };
  return folla;
}
