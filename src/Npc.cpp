#include "Npc.h"

#include <cmath>

#include "UIButton.h"
#include "UILabel.h"
#include "UIPanel.h"
#include "UIRow.h"
#include "UITextBlock.h"

using namespace std;
using namespace glm;

// Lines of the dialogue box
#define TEXT_LINES 4
// Where the player has to look at: chest height
#define CHEST_HEIGHT 0.9f

Npc::Npc(shared_ptr<AnimatedModel> model, const string &name,
         const vector<string> &lines, SoundEngine &engine,
         SpeechSynthesizer &synthesizer, const VoiceSettings &voiceSettings)
    : DynamicGameObject(model), name(name), lines(lines),
      voice(engine, synthesizer, voiceSettings) {}

void Npc::faceTowards(const vec3 &point) {
  vec3 d = point - position;
  if (d.x == 0.0f && d.z == 0.0f)
    return;
  // Same convention as Walker: yaw 0 looks towards +z
  facing = std::atan2(d.x, d.z);
  setYaw(facing);
}

void Npc::sayCurrent() {
  if (current < lines.size())
    voice.say(lines[current]);
}

void Npc::update(double dt) {
  DynamicGameObject::update(dt);
  voice.update(dt, position + vec3(0.0f, MOUTH_HEIGHT, 0.0f));
}

vec3 Npc::getInteractionPoint() const {
  return position + vec3(0.0f, CHEST_HEIGHT, 0.0f);
}

void Npc::buildInterface(UIPanel &panel) {
  // The line being said, revealed as the voice speaks it
  panel.add(new UITextBlock(
      [this]() { return current < lines.size() ? lines[current] : string(); },
      TEXT_LINES, [this]() { return voice.progress(); }));
  panel.add(new UILabel(
      [this]() {
        string where = to_string(current + 1) + "/" + to_string(lines.size());
        if (voice.getState() == Voice::State::Synthesizing)
          return where + "  ...";
        return where + (voice.isSpeaking() ? "  hablando" : "");
      },
      UITheme::MUTED));

  UIRow *buttons = panel.add(new UIRow());
  buttons->add(new UIButton("Repetir", [this]() { sayCurrent(); }));
  buttons->add(new UIButton("Siguiente", [this]() {
    if (lines.empty())
      return;
    current = (current + 1) % lines.size();
    sayCurrent();
  }));
  panel.add(new UILabel("Esc: terminar", UITheme::MUTED));
}

void Npc::onInterfaceOpened(const vec3 &playerPosition) {
  faceTowards(playerPosition);
  sayCurrent();
}

void Npc::onInterfaceClosed() { voice.stop(); }
