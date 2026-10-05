#include "Npc.h"

#include <cmath>

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

void Npc::update(double dt) {
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
