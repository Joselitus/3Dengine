#ifndef PINGU
#define PINGU

#include <memory>
#include <string>
#include <vector>

#include "Npc.h"

// Pingu, the penguin who watches the antenna: an Npc that dances by default
// and stands still, breathing calmly, while he talks to the player. He has
// both meshes loaded (DynamicGameObject::addMesh) and the animation changer is
// his onInteraction: the dialogue starting shows the standing one and its
// ending brings the dance back.
class Pingu : public Npc {
public:
  // Indices of his meshes (see DynamicGameObject::setMesh)
  enum Mesh { Dancing = 0, Standing = 1 };

  // `dancing` plays the dance and `standing` is the same model in its idle
  // pose (AnimatedModel::setIdle); both fitted to stand on his position
  Pingu(std::shared_ptr<AnimatedModel> dancing,
        std::shared_ptr<AnimatedModel> standing, const std::string &name,
        const std::vector<std::string> &lines, SoundEngine &engine,
        SpeechSynthesizer &synthesizer,
        const VoiceSettings &voiceSettings = VoiceSettings());

  // Talking: he stops dancing; the talk is over: he dances again
  void onInteraction(const Interaction &interaction) override;
};

#endif
