#ifndef NPC
#define NPC

#include <memory>
#include <string>
#include <vector>

#include "Dialogue.h"
#include "DynamicGameObject.h"
#include "Interactable.h"
#include "DeathPose.h"
#include "NpcRagdoll.h"
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
  glm::vec3 lastPlayerPosition = glm::vec3(0.0f);
  Voice voice;
  Dialogue dialogue; // after `voice`, which it speaks with
  float facing = 0.0f; // radians, around +y
  // Once it has turned into a ragdoll (startRagdoll)
  std::unique_ptr<NpcRagdoll> ragdoll;
  Ragdoll::FloorQuery ragdollFloor;
  std::function<bool(glm::vec3 &)> headHold;
  std::unique_ptr<DeathPose> death; // once it has been shot (startDeath)
  bool dead = false;
  bool wasHeld = false;       // its head was held and let go: it is itself again
  bool wasIdle = false;       // (its model was in the breathing pose before)
  Ragdoll::FloorQuery replicaFloor;
  float savedGravity = 0.0f;  // what it had before it was a ragdoll
  bool savedCollidable = true;
  void endRagdoll();

public:
  // What happened in an interaction with the player, for onInteraction()
  struct Interaction {
    enum class Type {
      Started,  // the player used the NPC: the dialogue box has just opened
      Finished, // the dialogue box has closed, however it was closed
    };
    Type type;
    glm::vec3 playerPosition; // where the player was (when it started)
  };

  // Height of the mouth above the position, where the voice comes from
  static constexpr float MOUTH_HEIGHT = 1.5f;

  Npc(std::shared_ptr<AnimatedModel> model, const std::string &name,
      const std::vector<std::string> &lines, SoundEngine &engine,
      SpeechSynthesizer &synthesizer,
      const VoiceSettings &voiceSettings = VoiceSettings());

  void faceTowards(const glm::vec3 &point);
  // Runs after the NPC has dealt with an interaction (turned to the player and
  // started the dialogue, or ended it). It does nothing; a subclass overrides
  // it to react to each one: change its animation, give something...
  virtual void onInteraction(const Interaction &interaction) {}
  const Voice &getVoice() const { return voice; }

  void update(double dt) override;
  // Shot: once its health is gone it dies (startDeath: it falls over backwards, for good)
  void takeDamage(float amount, const glm::vec3 &direction, const Stage &stage) override;
protected:
  float health = 1.0f;
public:

  // Turns into a ragdoll (see NpcRagdoll) from the pose it has now, for good: it stops talking, can
  // no longer be used, falls and lies on `floor`. While `holdHead` (asked every frame) returns true
  // and gives a point, its head is held there (in a mouth) and the rest of it hangs from it; when
  // that stops (the one holding it was run over...) it gets up where it fell: it is itself again, as
  // before (standing on its feet, usable, back to its animation: onRagdollEnded). Without `holdHead`
  // it stays a ragdoll for good.
  // False if its model can't be made into one (or it already is).
  bool startRagdoll(Ragdoll::FloorQuery floor, std::function<bool(glm::vec3 &)> holdHead = nullptr);
  // Dies: it stops talking and can no longer be used, and falls over backwards where it stands to lie
  // on its back for good (DeathPose: the same fall as the player's penguin)
  void startDeath();
  // Out of action: a ragdoll (carried by a creature or not) or dead
  bool isRagdolling() const { return ragdoll != nullptr || dead; }
  // A client: where a ragdoll falls to (the floor), for the copy of an NPC that was shot
  void setReplicaFloor(Ragdoll::FloorQuery floor) { replicaFloor = floor; }
  // The server tells the clients whether it lies for good (shot); one that a creature carries is
  // told by the creature
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  // It stopped being a ragdoll and is itself again (see startRagdoll: its head let go): a subclass
  // puts its animation back
  virtual void onRagdollEnded() {}
  // Where its head is held, if it is
  bool isHeadHeld() const { return ragdoll && holdHeadNow; }
private:
  bool holdHeadNow = false;
public:

  // Interactable
  bool isInteractionAvailable() const override { return !ragdoll && !dead; }
  std::string getInteractionName() const override { return name; }
  std::string getInteractionVerb() const override { return "hablar con"; }
  glm::vec3 getInteractionPoint() const override;
  void buildInterface(UIPanel &panel) override;
  void onInterfaceOpened(const glm::vec3 &playerPosition) override;
  void onInterfaceClosed() override;
};

#endif
