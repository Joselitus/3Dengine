// Pingu (the penguin NPC, assets/ping) for the creature testing tool: its recorded dance, and the
// ragdoll the creature turns it into when it catches it (NpcRagdoll), which can be held by the head.
#include <glm/gtc/matrix_transform.hpp>

#include "AnimatedModel.h"
#include "GameObject.h"
#include "NpcRagdoll.h"
#include "Stage.h"
#include "TestAnimation.h"

using namespace std;
using namespace glm;

namespace {

const char *MODEL_FILE = "../assets/ping/PenguinoAnimado.fbx";
const unsigned int PENGUIN_ANIMATION = 1; // (see TestStage: the first take is wrong)

mat3 yawAxes(float heading) { return mat3(rotate(mat4(1.0f), heading, vec3(0.0f, 1.0f, 0.0f))); }

class PinguAnimation : public TestAnimation {
protected:
  Stage &stage;
  shared_ptr<AnimatedModel> model;
  shared_ptr<GameObject> object;
  explicit PinguAnimation(Stage &stage) : stage(stage) {
    model = make_shared<AnimatedModel>(MODEL_FILE, true, PENGUIN_ANIMATION);
    object = make_shared<GameObject>(model);
    object->setCollidable(false);
    stage.add(object);
  }
};

// The dance he does in the game, recorded in the file
class DanceAnimation : public PinguAnimation {
public:
  explicit DanceAnimation(Stage &stage) : PinguAnimation(stage) {}
  string name() const override { return "dance (recorded)"; }
  void reset(const TestBody &) override {}
  void update(double, TestBody &body) override {
    object->setPosition(body.position.x, body.position.y, body.position.z);
    object->setYaw(body.heading);
  }
  void joints(vector<vec3> &out) const override {
    for (float y = 0.2f; y < 1.8f; y += 0.3f)
      out.push_back(object->getPosition() + vec3(0.0f, y, 0.0f));
  }
};

// The ragdoll: it starts from the pose the dance has when it begins, and falls; held by the mouse it
// hangs by the head from the point the cursor takes it to
class RagdollAnimation : public PinguAnimation {
  NpcRagdoll ragdoll;
  Ragdoll::FloorQuery floor;
  static constexpr float HOLD_HEIGHT = 2.1f; // where the head is while it is held

public:
  explicit RagdollAnimation(Stage &stage) : PinguAnimation(stage), ragdoll(model) {
    floor = [&stage](float x, float z, float &height) { return stage.floorAt(x, z, height); };
  }
  string name() const override { return "ragdoll (held by the head)"; }
  void reset(const TestBody &body) override {
    model->Update(0.7); // (a pose of the dance)
    ragdoll.start(body.position + vec3(0.0f, 1.2f, 0.0f), yawAxes(body.heading), vec3(0.0f));
  }
  void update(double dt, TestBody &body) override {
    if (body.held)
      ragdoll.holdHead(body.position + vec3(0.0f, HOLD_HEIGHT, 0.0f));
    else
      ragdoll.releaseHead();
    ragdoll.step(dt, floor, nullptr);
    vec3 pelvis = ragdoll.pelvis();
    float ground = 0.0f;
    stage.floorAt(pelvis.x, pelvis.z, ground);
    if (!body.held)
      body.position = vec3(pelvis.x, ground, pelvis.z);
    object->setPosition(pelvis.x, pelvis.y, pelvis.z);
    object->setRotation(mat4(1.0f));
    ragdoll.apply(pelvis);
  }
  void joints(vector<vec3> &out) const override {
    // (the pelvis and a line up from it: close enough to grab it by)
    vec3 p = ragdoll.pelvis();
    for (float y = -0.4f; y <= 0.8f; y += 0.3f)
      out.push_back(p + vec3(0.0f, y, 0.0f));
  }
  void skeleton(vector<pair<vec3, vec3>> &out) const override { ragdoll.segments(out); }
  bool walks() const override { return false; }
  string status() const override { return "ragdoll"; }
};

} // namespace

CreatureEntry pinguEntry() {
  CreatureEntry pingu;
  pingu.name = "pingu";
  pingu.animations = {"dance (recorded)", "ragdoll (held by the head)"};
  pingu.walkSpeed = 1.5f;
  pingu.create = [](int index, Stage &stage) -> unique_ptr<TestAnimation> {
    if (index == 0)
      return unique_ptr<TestAnimation>(new DanceAnimation(stage));
    return unique_ptr<TestAnimation>(new RagdollAnimation(stage));
  };
  return pingu;
}
