#include "Npc.h"

#include <cmath>

#include "Stage.h"

using namespace std;
using namespace glm;

// Where the player has to look at: chest height
#define CHEST_HEIGHT 0.9f

// How fast an NPC loses the horizontal velocity a push gave it (1 / seconds)
#define NPC_DRAG 10.0f

Npc::Npc(shared_ptr<AnimatedModel> model, const string &name,
         const vector<string> &lines, SoundEngine &engine,
         SpeechSynthesizer &synthesizer, const VoiceSettings &voiceSettings)
    : DynamicGameObject(model), name(name),
      voice(engine, synthesizer, voiceSettings),
      dialogue(lines, voice, "hablando") {
  // Nobody drives it: when something pushes it, it must stop soon after
  // instead of sliding away (a velocity dies out in about 1 / NPC_DRAG s)
  setDrag(NPC_DRAG);
}

void Npc::faceTowards(const vec3 &point) {
  vec3 d = point - position;
  if (d.x == 0.0f && d.z == 0.0f)
    return;
  // Same convention as Walker: yaw 0 looks towards +z
  facing = std::atan2(d.x, d.z);
  setYaw(facing);
}

void Npc::takeDamage(float amount, const vec3 &direction, const Stage &stage) {
  if (ragdoll || health <= 0.0f)
    return;
  health -= amount;
  if (health > 0.0f)
    return;
  velocity += normalize(direction) * SHOT_KNOCK;
  startRagdoll([&stage](float x, float z, float &height) { return stage.floorAt(x, z, height); });
}

bool Npc::startRagdoll(Ragdoll::FloorQuery floor, function<bool(vec3 &)> holdHead) {
  if (ragdoll || !aniModel)
    return false;
  unique_ptr<NpcRagdoll> body(new NpcRagdoll(aniModel));
  if (!body->isValid())
    return false;
  voice.stop(); // (dead people do not talk)
  wasIdle = aniModel->isIdle();
  savedGravity = getGravity();
  savedCollidable = isCollidable();
  wasHeld = false;
  body->start(position, mat3(rotation), velocity);
  if (aniModel->isIdle()) // (its breathing pose is made by the shader: the ragdoll poses the bones instead)
    aniModel->setIdle(false);
  ragdoll = std::move(body);
  ragdollFloor = floor;
  headHold = holdHead;
  // what moves it now is the ragdoll: no gravity, no collisions, and the object is where its pelvis is
  velocity = acceleration = vec3(0.0f);
  setGravity(0.0f);
  setCollidable(false);
  return true;
}

// Back from the ragdoll: standing where it fell, with its animation, its gravity and its collisions
void Npc::endRagdoll() {
  vec3 where = ragdoll->pelvis();
  ragdoll.reset();
  holdHeadNow = false;
  wasHeld = false;
  headHold = nullptr;
  float ground = where.y;
  if (ragdollFloor)
    ragdollFloor(where.x, where.z, ground);
  position = vec3(where.x, ground, where.z);
  velocity = acceleration = vec3(0.0f);
  rotation = mat4(1.0f);
  setYaw(facing);
  setGravity(savedGravity);
  setCollidable(savedCollidable);
  if (wasIdle)
    aniModel->setIdle(true); // (its breathing pose again)
  aniModel->usePlayedAnimation();
  onRagdollEnded();
}

void Npc::update(double dt) {
  if (ragdoll) {
    vec3 hold;
    holdHeadNow = headHold && headHold(hold);
    if (holdHeadNow) {
      ragdoll->holdHead(hold);
      wasHeld = true;
    } else {
      if (wasHeld) { // it was held and let go (the holder is gone): it is itself again
        endRagdoll();
        return;
      }
      ragdoll->releaseHead();
    }
    ragdoll->step(dt, ragdollFloor, nullptr);
    position = ragdoll->pelvis();
    rotation = mat4(1.0f);
    ragdoll->apply(position);
    return;
  }
  DynamicGameObject::update(dt);
  voice.update(dt, position + vec3(0.0f, MOUTH_HEIGHT, 0.0f));
}

vec3 Npc::getInteractionPoint() const {
  return position + vec3(0.0f, CHEST_HEIGHT, 0.0f);
}

void Npc::buildInterface(UIPanel &panel) { dialogue.buildPanel(panel); }

void Npc::onInterfaceOpened(const vec3 &playerPosition) {
  lastPlayerPosition = playerPosition;
  faceTowards(playerPosition);
  dialogue.start();
  onInteraction({Interaction::Type::Started, playerPosition});
}

void Npc::onInterfaceClosed() {
  dialogue.end();
  onInteraction({Interaction::Type::Finished, lastPlayerPosition});
}
