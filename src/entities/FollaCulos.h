#ifndef FOLLA_CULOS
#define FOLLA_CULOS

#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "AudioClip.h"
#include "Npc.h"
#include "SoundEngine.h"
#include "Ragdoll.h"
#include "SpiderGait.h"
#include "SpotLight.h"

// The night creature (assets/folla_culos): gaunt, ash white, with yellow glowing eyes. It
// walks on four legs. Its behaviour is a small state machine (enum Behavior), asked every frame:
//
//   RunAway --(night falls)--> Pursuit --(the player is in the vehicle and is within
//   CAUTIONARY_RANGE)--> Caution --(the player gets out)--> Pursuit;  from any state, (the sun
//   rises) --> RunAway;  at night, from Pursuit or Caution, (another NPC is within
//   CAUTION_MIN_RADIUS of it) --> Hunt --(it lost it)--> Pursuit;  Hunt --(it caught it)-->
//   RunAway, by day or by night, until it is FLEE_DISTANCE from the player (then the usual rules).
//
//  - Pursuit: it runs in a straight line towards the playable character, the one the player is
//    controlling at that moment (the stage gives it the way to ask for the character's position:
//    setTarget), and stops when it reaches it (STOP_DISTANCE): if the player is on foot that is touching
//    him: he dies (the stage's callback) and the creature runs away (RunAway until FLEE_DISTANCE; and
//    it stays in RunAway while the player is dead).
//  - Caution: the player is in the vehicle, which it does not dare to go near: it keeps to the
//    edge of the imaginary circle of CAUTIONARY_RANGE metres around the player and never steps
//    into it. It walks along the edge, going round the circle by its arc, to a random point of
//    it, stops a moment looking at the player, and walks round to another. If the circle moves
//    (the player drives) it goes with it: it runs in to the edge if it is left outside, and out
//    to it if the player drives at it.
//  - RunAway (the sun is up): it runs away from the player in a straight line until it is
//    FLEE_DISTANCE away, and stops.
//  - Hunt: another NPC (an Npc other than the player) came closer than CAUTION_MIN_RADIUS: it
//    targets that one instead and runs at it. When it reaches it (CATCH_DISTANCE: they collide) the
//    NPC turns into a ragdoll (Npc::startRagdoll) and its head is held in the creature's mouth, the
//    body hanging from it and dragging behind, for as long as the creature lives. With its prey it
//    retreats: it goes into RunAway whatever the time of day, until it is FLEE_DISTANCE from the player.
// Whether it is night is given by the stage (setNightQuery), and whether the player is in the
// vehicle too (setPlayerInVehicleQuery). Its pose is not an animation: it is made by code every
// frame (SpiderGait): the body moves and its four feet stay planted on the ground, and each one
// lifts and steps forward when the body has gone too far past it.
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
  std::function<bool()> isNight;
  std::function<glm::vec3()> lookTarget;
  std::function<bool()> playerInVehicle;
  std::function<void()> playerCaught;    // it touched the player on foot
  std::function<bool()> playerDead;
  std::function<std::vector<Npc *>()> otherNpcs;
  Npc *prey = nullptr;                 // the NPC it is hunting
  bool retreating = false;             // it has just caught one: it runs away, whatever the time of day
  std::vector<Npc *> devoured;         // the ones whose heads it carries
  std::map<std::string, glm::mat4> lastPose; // the pose it has now (to know where its mouth is)
  Npc *findPrey();
  glm::vec3 mouthPosition() const;
  void devour(Npc &npc);
  // Its screech while it chases: it may start at any frame (see updateScreech)
  SoundEngine &soundEngine;
  std::shared_ptr<AudioClip> screechClip;
  std::unique_ptr<Sound> screech;
  std::mt19937 random;
  bool pursuing = false; // running at the target (not away from it)
public:
  // What it is doing (see the class comment)
  enum class Behavior { Pursuit, Caution, RunAway, Hunt };

private:
  Behavior behavior = Behavior::Pursuit;
  float cautionAngle = 0.0f; // the point of the circle it is walking to in Caution (an angle round the player)
  bool hasCautionGoal = false;
  float cautionPause = 0.0f; // seconds left standing still, looking at the player
  void enterBehavior(Behavior next);
  Behavior nextBehavior(float distance) const;
  glm::vec3 cautionMove(const glm::vec3 &target, double dt);
  void updateScreech(double dt);
  bool running = false;
  bool dead = false;
  bool criticalCondition = false; // stuck on the windshield of the vehicle that hit it, dying
  bool stuck = false;
  bool ragdolling = false;
  Ragdoll ragdoll;
  glm::vec3 lastPosition = glm::vec3(0.0f), gaitVelocity = glm::vec3(0.0f); // how fast it really moves
  bool hasLastPosition = false;
  SpiderGait gait; // how it walks: four legs that step, made by code (it has no running animation)
  std::shared_ptr<AnimatedModel> runningModel, splatModel; // (to switch the glow of the eyes off)
  Ragdoll::FloorQuery floorHeight;
  Ragdoll::PushOut pushOut;
  std::function<glm::vec3()> carrierVelocity; // the vehicle it is stuck on, for when it lets go
  double splatClock = 0.0;
  // Was the collision that killed it a front one of a vehicle? (asked once, when it dies)
  std::function<bool(const FollaCulos &)> frontHit;
  // The surface it sticks to: middle, way up the slope and outward normal
  std::function<void(glm::vec3 &, glm::vec3 &, glm::vec3 &)> surfaceFrame;

public:
  // How fast it runs (the old running animation's speed: generate_folla_culos_run.py, SPEED)
  static constexpr float RUN_SPEED = 4.34f;
  // The screech is played louder than recorded (1 = as recorded; the file's peak is 0.77, so
  // 1.3 brings it to full scale)
  static constexpr float SCREECH_VOLUME = 1.3f;
  // The chance, per second, that it starts a screech while it is chasing and none is playing
  // (applied each frame as 1 - (1 - p)^dt, so it does not depend on the frame rate)
  static constexpr float SCREECH_CHANCE_PER_SECOND = 0.12f;
  // In Caution it keeps to the edge of the circle of this radius round the player: three lengths
  // of the RV (7.4 m)
  static constexpr float CAUTIONARY_RANGE = 22.2f;
  // ...walking along it at this speed (it runs to get back to the edge when it is more than
  // CAUTION_EDGE_TOLERANCE metres from it), to a point a random angle (radians, between these
  // two) further round, and standing still a random time between these two (seconds) there
  static constexpr float CAUTION_WALK_SPEED = 1.8f;
  static constexpr float CAUTION_EDGE_TOLERANCE = 1.5f;
  static constexpr float CAUTION_ARC_MIN = 0.35f, CAUTION_ARC_MAX = 2.0f;
  static constexpr float CAUTION_PAUSE_MIN = 0.5f, CAUTION_PAUSE_MAX = 2.0f;
  // Another NPC closer than this (m) to it is prey: it hunts it (and keeps on while it is within 1.5
  // times as far), and it has caught it when it is within CATCH_DISTANCE (m)
  static constexpr float CAUTION_MIN_RADIUS = 7.8f;
  static constexpr float CATCH_DISTANCE = 1.3f;
  // It has touched the player on foot at this distance (m): their bodies stop each other a little
  // before STOP_DISTANCE (the creature pushes him along rather than closing in), so it is a bit more
  static constexpr float PLAYER_CATCH_DISTANCE = 1.8f;
  // Its mouth in the model's bind pose (the middle of the mouth's mesh), and its head's joint
  static constexpr float MOUTH_BIND_Y = 2.072f, MOUTH_BIND_Z = 0.126f, HEAD_JOINT_Y = 2.05f, HEAD_JOINT_Z = 0.02f;
  // It stops when it is this close to the target
  static constexpr float STOP_DISTANCE = 1.3f;
  // By day it runs away until it is this far from the target
  static constexpr float FLEE_DISTANCE = 90.0f;
  // Where the glow of the eyes comes from, above its position
  static constexpr float EYE_HEIGHT = 1.45f;
  // How far the light of its eyes reaches (metres)
  static constexpr float EYE_LIGHT_RANGE = 2.5f;
  // A collision that changes its horizontal velocity by more than this (m/s) kills it. On foot
  // nothing comes near it (the player walks at 4 m/s and shares the push: ~2); a vehicle does, even
  // the RV on sand (top speed 10) running after it as it runs away at 4.3 (a closing speed of 5.7)
  static constexpr float DEATH_SPEED_CHANGE = 5.0f;

  // Mesh 0 is the running one and mesh 1 the "splat" one (the same file, animation 1)
  enum Mesh { Running = 0, Splat = 1 };
  // The point of its body (its height, a bit below the head, so that the face shows in the glass
  // and not above the roof) that is put in the middle of the surface it is stuck on, how far the
  // body sticks out of it, and how much it is turned about the normal (degrees)
  static constexpr float ANCHOR_HEIGHT = 1.95f;
  static constexpr float STUCK_OFFSET = 0.12f;
  static constexpr float STUCK_ROLL = 10.0f;
  // It lets go of the windshield when the vehicle goes slower than this (m/s)
  static constexpr float RAGDOLL_SPEED = 3.0f;

  FollaCulos(std::shared_ptr<AnimatedModel> running,
             std::shared_ptr<AnimatedModel> splat, SoundEngine &engine,
             SpeechSynthesizer &synthesizer);

  // Where to run to (asked every frame, so it follows whoever the player controls)
  void setTarget(std::function<glm::vec3()> where) { targetPosition = where; }
  // Whether the player is inside the vehicle (asked every frame); without it, never
  void setPlayerInVehicleQuery(std::function<bool()> inVehicle) { playerInVehicle = inVehicle; }
  Behavior getBehavior() const { return behavior; }
  // What it does when it touches the player on foot (the stage kills him), and whether he is dead
  // already (then it only runs away); without them it only stops next to him
  void setPlayerCaughtCallback(std::function<void()> caught) { playerCaught = caught; }
  void setPlayerDeadQuery(std::function<bool()> dead) { playerDead = dead; }
  // The other NPCs that can be its prey (asked every frame); without it, it hunts nobody
  void setPreyQuery(std::function<std::vector<Npc *>()> npcs) { otherNpcs = npcs; }
  size_t getDevouredCount() const { return devoured.size(); }
  // Whether it is night (asked every frame); without it, it is always night
  void setNightQuery(std::function<bool()> night) { isNight = night; }
  // Where its face looks while it walks (asked every frame: the camera); without it, forwards
  void setLookTarget(std::function<glm::vec3()> where) { lookTarget = where; }
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

private:
  void die(); // the eyes stop glowing
public:
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
  // Shot: once its health is gone it dies (kill)
  void takeDamage(float amount, const glm::vec3 &direction, const Stage &stage) override;
  // The light of its eyes
  void getLight(std::vector<SpotLight> &lights) const;
  void describe(std::vector<std::string> &lines) const override;
};

#endif
