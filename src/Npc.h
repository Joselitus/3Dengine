#ifndef NPC
#define NPC

#include <memory>
#include <string>
#include <vector>

#include "DynamicGameObject.h"
#include "Interactable.h"
#include "Voice.h"

// A character the player can talk to. It has a name, lines of dialogue and
// a Voice. Using it (the Use key) opens a dialogue panel: the NPC turns to
// face the player and says the current line out loud (text to speech, heard
// from where it stands), while the panel shows the line as it is spoken.
// "Siguiente" goes to the next line, "Repetir" says it again; closing the
// panel silences it. The next conversation starts where it was left.
//
// A DynamicGameObject, so the stage keeps it on the floor (give it gravity)
// and it could walk later. Its AnimatedModel should be fitted with
// feetAtOrigin, so it stands on its position.
class Npc : public DynamicGameObject, public Interactable {
private:
  std::string name;
  std::vector<std::string> lines; // UTF-8; the panel shows them in ASCII
  size_t current = 0;
  Voice voice;
  float facing = 0.0f; // radians, around +y

  void sayCurrent();

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
  glm::vec3 getInteractionPoint() const override;
  void buildInterface(UIPanel &panel) override;
  void onInterfaceOpened(const glm::vec3 &playerPosition) override;
  void onInterfaceClosed() override;
};

#endif
