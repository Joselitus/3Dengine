#ifndef FOLLA_CULOS
#define FOLLA_CULOS

#include <functional>
#include <memory>
#include <string>

#include "Npc.h"
#include "Ragdoll.h"
#include "SpotLight.h"

// The night creature (assets/folla_culos): gaunt, ash white, with yellow glowing eyes. It
// runs on four legs. For now its behaviour is only this: at night it runs in a straight line
// towards the playable character, the one the player is controlling at that moment (the
// stage gives it the way to ask for the character's position: setTarget), and stops when it
// reaches it; by day it runs away from it in a straight line, and stops when it is far
// (FLEE_DISTANCE). Whether it is night is also given by the stage (setNightQuery). Its running animation moves in place at RUN_SPEED, so it moves at that
// speed for its feet not to slip.
//
// It can be killed: if a collision flings it too fast (the RV running it over), it stays in the
// stage, loaded, as a GameObject (it no longer runs or collides), and:
//  - if the collision was a front one of the vehicle (frontHit says so), it ends up stuck on
//    its windshield, spread out like a dead bug and breathing slowly (its "splat" animation,
//    mesh 1), still alive: `criticalCondition` is true and `dead` false, it follows the vehicle
//    (surfaceFrame gives the windshield);
//  - when the vehicle slows below RAGDOLL_SPEED (carrierVelocity) it lets go: `criticalCondition`
//    goes false, `dead` true, and it falls as a ragdoll (a Ragdoll of its skeleton: it is
//    simulated with the floor and the vehicle's body, and poses the running model's bones);
//  - if the collision was not a front one it just dies (`dead`) and is no longer drawn.
//
// It is an Npc (with no dialogue: it is not an Interactable of the map, it can't be talked
// to). Its eyes glow (emissive material) and it also gives off a little yellow light.
class FollaCulos : public Npc {
private:
  std::function<glm::vec3()> targetPosition;
  double runClock = 0.0; // the running animation only advances while it runs
  std::function<bool()> isNight;
  bool running = false;
  bool dead = false;
  bool criticalCondition = false; // stuck on the windshield of the vehicle that hit it, dying
  bool stuck = false;
  bool ragdolling = false;
  Ragdoll ragdoll;
  Ragdoll::FloorQuery floorHeight;
  Ragdoll::PushOut pushOut;
  std::function<glm::vec3()> carrierVelocity; // the vehicle it is stuck on, for when it lets go
  double splatClock = 0.0;
  // Was the collision that killed it a front one of a vehicle? (asked once, when it dies)
  std::function<bool(const FollaCulos &)> frontHit;
  // The surface it sticks to: middle, way up the slope and outward normal
  std::function<void(glm::vec3 &, glm::vec3 &, glm::vec3 &)> surfaceFrame;

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
  // A collision that changes its horizontal velocity by more than this (m/s) kills it: it
  // runs at 4.3, so a push this size is far more than anything on foot, only a vehicle does it
  static constexpr float DEATH_SPEED_CHANGE = 8.0f;

  // Mesh 0 is the running one and mesh 1 the "splat" one (the same file, animation 1)
  enum Mesh { Running = 0, Splat = 1 };
  // How high its chest is, and how far the chest sticks out of the surface it is stuck on
  static constexpr float CHEST_HEIGHT = 1.45f;
  static constexpr float STUCK_OFFSET = 0.12f;
  // It lets go of the windshield when the vehicle goes slower than this (m/s)
  static constexpr float RAGDOLL_SPEED = 3.0f;

  FollaCulos(std::shared_ptr<AnimatedModel> running,
             std::shared_ptr<AnimatedModel> splat, SoundEngine &engine,
             SpeechSynthesizer &synthesizer);

  // Where to run to (asked every frame, so it follows whoever the player controls)
  void setTarget(std::function<glm::vec3()> where) { targetPosition = where; }
  // Whether it is night (asked every frame); without it, it is always night
  void setNightQuery(std::function<bool()> night) { isNight = night; }
  bool isRunning() const { return running; }
  bool isDead() const { return dead; }
  // Dead and stuck on the windshield (see the class comment)
  bool isCriticalCondition() const { return criticalCondition; }
  bool isStuck() const { return stuck; }
  bool isRagdolling() const { return ragdolling; }
  // How it falls: the floor, the vehicle's body to fall on, and how fast the vehicle goes (all
  // asked every frame)
  void setRagdollWorld(Ragdoll::FloorQuery floor, Ragdoll::PushOut push,
                       std::function<glm::vec3()> carrierVelocity) {
    floorHeight = floor;
    pushOut = push;
    this->carrierVelocity = carrierVelocity;
  }
  // Lets go now (what slowing down does): dead, and a ragdoll
  void startRagdoll();
  void setFrontHitTest(std::function<bool(const FollaCulos &)> test) { frontHit = test; }
  void setSurfaceFrame(std::function<void(glm::vec3 &, glm::vec3 &, glm::vec3 &)> frame) {
    surfaceFrame = frame;
  }
  // It dies (what a too fast collision does): dead = true and it is no longer drawn
  void kill();

  void update(double dt) override;
  // The stage tells it how a collision moved it: too fast kills it (floor contacts are only
  // vertical, they never do)
  void applyCollision(const glm::vec3 &push, const glm::vec3 &velocityChange) override;
  // The light of its eyes
  void getLight(std::vector<SpotLight> &lights) const;
  void describe(std::vector<std::string> &lines) const override;
};

#endif
