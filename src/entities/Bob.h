#ifndef BOB
#define BOB

#include <functional>
#include <memory>
#include <random>

#include "DynamicGameObject.h"
#include "SpotLight.h"

class Saucer;

// Bob, the alien (assets/bob, generate_bob.py): a grey, 1.45 m tall, with a big head and huge
// almond eyes, black by day and glowing white at night. He only comes at night, in his ship
// (Saucer), and only to the forests. What he does (enum Behavior):
//
//   Inside (in the ship: not there) --(the ship's ramp is down: disembark)--> Exiting (down the
//   ramp) --> Prowl (he walks about near the ship, stops and stares at the player) --(the player
//   on foot within CHASE_RANGE)--> Chase --(the player gets away beyond LOSE_RANGE, or gets into
//   a vehicle)--> Prowl.
//   Chase --(now and then, with the player between RAY_MIN and RAY_MAX)--> Firing: he stops and
//   shoots a yellow ray from his eyes that paralyses the player for PARALYSIS_TIME (the stage's
//   callback); then he goes on chasing.
//   Chase --(he reaches him: CATCH_DISTANCE)--> if the player is paralysed he takes him (the
//   stage's callback: he is abducted, the end); if not, Grabbing: he holds him for GRAB_TIME, and
//   the player can hammer the leave-vehicle key (struggle) to get free: if he does, Bob falls over
//   (Fallen: on his back, then he gets up) and leaves him alone for a while; if not, he takes him.
//   At dawn (or once he has the player), any --> Returning (to the foot of the ramp; he waits
//   there until it is down) --> Boarding (up the ramp) --> Inside (the ship closes and goes:
//   Saucer::boarded).
//
// His body is a model and his limbs eight more (upper arm, forearm, thigh, shin on each side), all
// in his body's frame: they swing about their joints as he walks (a procedural walk: hips and
// shoulders swing in opposition, knees bend on the forward swing, the step follows the distance
// he covers), and his arms reach out when he holds the player. His eyes are two models: black
// ones, and white ones drawn glowing (at night). The ray is one more model (a thin yellow
// glowing rod, one per eye) stretched from each eye to the player's head.
class Bob : public DynamicGameObject {
public:
  enum class Behavior { Inside, Exiting, Prowl, Chase, Firing, Grabbing, Fallen, Returning, Boarding };

  static constexpr float WALK_SPEED = 1.3f, CHASE_SPEED = 3.8f; // m/s (the player walks at 4)
  static constexpr float CHASE_RANGE = 35.0f, LOSE_RANGE = 60.0f;
  static constexpr float CATCH_DISTANCE = 1.1f;
  static constexpr float PROWL_RADIUS = 12.0f; // round the foot of the ramp
  static constexpr float RAMP_SPEED = 0.9f;    // m/s, up and down the ramp
  static constexpr float STRIDE = 1.0f;        // metres per step cycle (two steps)
  // The ray: from this far (m), the chance per second that he fires while he chases, how long it
  // charges and how long it shines (s), how long it paralyses (s), and how long until he fires
  // again (s)
  static constexpr float RAY_MIN = 4.0f, RAY_MAX = 18.0f, RAY_CHANCE = 0.4f;
  static constexpr float RAY_CHARGE = 0.5f, RAY_TIME = 0.9f, PARALYSIS_TIME = 5.0f;
  static constexpr float RAY_COOLDOWN = 10.0f;
  // Holding the player: how long he can struggle (s), how much each press frees him (of 1) and
  // how fast that wears off (1/s); once he is free Bob lies FALLEN_TIME (s) and leaves him alone
  // for LEAVE_ALONE (s)
  static constexpr float GRAB_TIME = 4.0f, STRUGGLE_PER_PRESS = 0.11f, STRUGGLE_DECAY = 0.35f;
  static constexpr float FALLEN_TIME = 4.0f, LEAVE_ALONE = 6.0f;
  // The player's head over his feet (m), what the ray aims at
  static constexpr float HEAD_HEIGHT = 1.5f;

private:
  Behavior behavior = Behavior::Inside;
  float stateTime = 0.0f;
  Saucer *ship = nullptr;
  glm::vec3 goal = glm::vec3(0.0f);
  bool hasGoal = false;
  float pause = 0.0f;        // (Prowl) seconds left standing still
  float yaw = 0.0f;
  float walkPhase = 0.0f;    // radians: where he is in his step
  float walkAmount = 0.0f;   // 0 = standing .. 1 = full stride
  float reach = 0.0f;        // 0 = arms down .. 1 = reaching for the player
  float fall = 0.0f;         // 0 = standing .. 1 = on his back
  glm::vec3 lastPosition = glm::vec3(0.0f);
  bool tookPlayer = false;
  float rayCooldown = 0.0f;
  bool rayHit = false;       // (Firing) the paralysis has been given
  float struggle = 0.0f;     // (Grabbing) 0..1: at 1 the player is free
  float leaveAlone = 0.0f;   // seconds until he chases again
  std::mt19937 random;

  std::function<glm::vec3()> targetPosition;
  std::function<bool()> playerInVehicle, playerDead, playerParalysed, isNight;
  std::function<void()> takePlayer;
  std::function<void(float)> paralyse;

  size_t eyesPart = 0, glowPart = 0;
  size_t rayParts[2] = {0, 0};
  size_t limbParts[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // upper arm, forearm, thigh, shin; left, right

  void enter(Behavior next);
  void walkTo(const glm::vec3 &point, float speed, double dt);
  void faceTowards(const glm::vec3 &point, double dt);
  void animate(double dt);
  void aimRays(bool shining);
  void take();
  float uniform(float a, float b) { return std::uniform_real_distribution<float>(a, b)(random); }

public:
  // `limbs`: upper arm L, forearm L, thigh L, shin L, then the same for the right side; `ray`:
  // the rod of the ray (bob_ray.obj: along +z, 1 m long)
  Bob(std::shared_ptr<Model> body, std::shared_ptr<Model> eyes, std::shared_ptr<Model> eyesGlow,
      const std::vector<std::shared_ptr<Model>> &limbs, std::shared_ptr<Model> ray);

  void setShip(Saucer *saucer) { ship = saucer; }
  void setTarget(std::function<glm::vec3()> where) { targetPosition = where; }
  void setPlayerInVehicleQuery(std::function<bool()> q) { playerInVehicle = q; }
  void setPlayerDeadQuery(std::function<bool()> q) { playerDead = q; }
  void setPlayerParalysedQuery(std::function<bool()> q) { playerParalysed = q; }
  void setNightQuery(std::function<bool()> q) { isNight = q; }
  // What happens when he takes the player (the stage: he is abducted)
  void setTakePlayerCallback(std::function<void()> take) { takePlayer = take; }
  // What his ray does to the player (the stage paralyses him for that many seconds)
  void setParalyseCallback(std::function<void(float)> p) { paralyse = p; }

  // The ship has landed and its ramp is down: he comes out
  void disembark();
  // The player hammers the key while Bob holds him
  void struggleOnce();
  // He holds the player: how near the player is to getting free (0..1), or < 0
  float struggleProgress() const { return behavior == Behavior::Grabbing ? struggle : -1.0f; }
  bool isHolding() const { return behavior == Behavior::Grabbing; }
  Behavior getBehavior() const { return behavior; }
  bool isOut() const { return behavior != Behavior::Inside; }

  void update(double dt) override;
  void teleport(const glm::vec3 &position) override;
  float getHeading() const override { return yaw; }
  // His ray's yellow light, while it shines
  void getLight(std::vector<SpotLight> &lights) const;
  void describe(std::vector<std::string> &lines) const override;
};

#endif
