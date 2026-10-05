#include "Pingu.h"

using namespace std;

Pingu::Pingu(shared_ptr<AnimatedModel> dancing,
             shared_ptr<AnimatedModel> standing, const string &name,
             const vector<string> &lines, SoundEngine &engine,
             SpeechSynthesizer &synthesizer, const VoiceSettings &voiceSettings)
    : Npc(dancing, name, lines, engine, synthesizer, voiceSettings) {
  addMesh(standing); // mesh 1 (the dance, which he starts with, is 0)
}

void Pingu::onInteraction(const Interaction &interaction) {
  setMesh(interaction.type == Interaction::Type::Started ? Standing : Dancing);
}
