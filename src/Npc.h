#ifndef NPC
#define NPC

#include <memory>
#include <string>
#include <vector>

#include "Dialogue.h"
#include "DynamicGameObject.h"
#include "Interactable.h"
#include "Voice.h"

// A character the player can talk to: a Dialogue spoken by its Voice (text
// to speech, heard from where it stands). Using it (the Use key) opens the
// dialogue box; the NPC turns to face the player and says each line out
// loud while the box shows it as it is spoken (see Dialogue for the box:
// "Siguiente"/"Cerrar", Esc). Closing the box cuts the voice. For things
// that are read silently, see Readable.
//
// A DynamicGameObject, so the stage keeps it on the floor (give it gravity)
// and it could walk later. Its AnimatedModel should be fitted with
// feetAtOrigin, so it stands on its position.
class Npc : public DynamicGameObject, public Interactable {
private:
  std::string name;
  Voice voice;
  Dialogue dialogue; // after `voice`, which it speaks with
  float facing = 0.0f; // radians, around +y

public:
  // Height of the mouth above the position, where the voice comes from
  static constexpr float MOUTH_HEIGHT = 1.5f;

  Npc(std::shared_ptr<AnimatedModel> model, const std::string &name,
      const std::vector<std::string> &lines, SoundEngine &engine,
      SpeechSynthesizer &synthesizer,
      const VoiceSettings &voiceSettings = VoiceSettings());

  void faceTowards(const glm::vec3 &point);
  const Voice &getVoice() const { return voice; }

  void update(double dt) override;

  // Interactable
  std::string getInteractionName() const override { return name; }
  std::string getInteractionVerb() const override { return "hablar con"; }
  glm::vec3 getInteractionPoint() const override;
  void buildInterface(UIPanel &panel) override;
  void onInterfaceOpened(const glm::vec3 &playerPosition) override;
  void onInterfaceClosed() override;
};

#endif
