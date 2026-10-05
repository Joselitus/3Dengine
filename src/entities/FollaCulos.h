#ifndef FOLLA_CULOS
#define FOLLA_CULOS

#include <functional>
#include <memory>
#include <string>

#include "Npc.h"
#include "SpotLight.h"

// The night creature (assets/folla_culos): gaunt, ash white, with yellow glowing eyes. It
// runs on four legs. For now its behaviour is only this: at night it runs in a straight line
// towards the playable character, the one the player is controlling at that moment (the
// stage gives it the way to ask for the character's position: setTarget), and stops when it
// reaches it; by day it runs away from it in a straight line, and stops when it is far
// (FLEE_DISTANCE). Whether it is night is also given by the stage (setNightQuery). Its running animation moves in place at RUN_SPEED, so it moves at that
// speed for its feet not to slip.
//
// It is an Npc (with no dialogue: it is not an Interactable of the map, it can't be talked
// to). Its eyes glow (emissive material) and it also gives off a little yellow light.
class FollaCulos : public Npc {
private:
  std::function<glm::vec3()> targetPosition;
  double runClock = 0.0; // the running animation only advances while it runs
  std::function<bool()> isNight;
  bool running = false;

public:
  // How fast the animation makes it run (assets/folla_culos/generate_folla_culos_run.py: SPEED)
  static constexpr float RUN_SPEED = 4.34f;
  // It stops when it is this close to the target
  static constexpr float STOP_DISTANCE = 1.3f;
  // By day it runs away until it is this far from the target
  static constexpr float FLEE_DISTANCE = 90.0f;
  // Where the glow of the eyes comes from, above its position
  static constexpr float EYE_HEIGHT = 1.45f;
  // How far the light of its eyes reaches (metres)
  static constexpr float EYE_LIGHT_RANGE = 2.5f;

  FollaCulos(std::shared_ptr<AnimatedModel> model, SoundEngine &engine,
             SpeechSynthesizer &synthesizer);

  // Where to run to (asked every frame, so it follows whoever the player controls)
  void setTarget(std::function<glm::vec3()> where) { targetPosition = where; }
  // Whether it is night (asked every frame); without it, it is always night
  void setNightQuery(std::function<bool()> night) { isNight = night; }
  bool isRunning() const { return running; }

  void update(double dt) override;
  // The light of its eyes
  void getLight(std::vector<SpotLight> &lights) const;
  void describe(std::vector<std::string> &lines) const override;
};

#endif
