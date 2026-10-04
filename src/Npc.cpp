#include "Npc.h"

#include <cmath>

using namespace std;
using namespace glm;

// Where the player has to look at: chest height
#define CHEST_HEIGHT 0.9f

Npc::Npc(shared_ptr<AnimatedModel> model, const string &name,
         const vector<string> &lines, SoundEngine &engine,
         SpeechSynthesizer &synthesizer, const VoiceSettings &voiceSettings)
    : DynamicGameObject(model), name(name),
      voice(engine, synthesizer, voiceSettings),
      dialogue(lines, voice, "hablando") {}

void Npc::faceTowards(const vec3 &point) {
  vec3 d = point - position;
  if (d.x == 0.0f && d.z == 0.0f)
    return;
  // Same convention as Walker: yaw 0 looks towards +z
  facing = std::atan2(d.x, d.z);
  setYaw(facing);
}

void Npc::update(double dt) {
  DynamicGameObject::update(dt);
  voice.update(dt, position + vec3(0.0f, MOUTH_HEIGHT, 0.0f));
}

vec3 Npc::getInteractionPoint() const {
  return position + vec3(0.0f, CHEST_HEIGHT, 0.0f);
}

void Npc::buildInterface(UIPanel &panel) { dialogue.buildPanel(panel); }

void Npc::onInterfaceOpened(const vec3 &playerPosition) {
  faceTowards(playerPosition);
  dialogue.start();
}

void Npc::onInterfaceClosed() { dialogue.end(); }
