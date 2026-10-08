#ifndef MOSQUITO
#define MOSQUITO

#include <functional>
#include <memory>
#include <random>

#include "BuzzSynth.h"
#include "DynamicGameObject.h"
#include "InsectLegs.h"
#include "ParticleEmitter.h"
#include "SoundEngine.h"
#include "SpotLight.h"

class RV;

// A giant mosquito (assets/mosquito), an enemy that flies. Its body is one model, its wings two
// more and its six legs 24 more (4 segments each), all parts of the object: the wings flap about
// their hinges on the thorax and the legs are an InsectLegs (a ProceduralPose): chains of joints
// with mass that hang from the body and are pulled by springs towards a pose, so they lag, swing
// and settle as it flies (inertia). It buzzes all the time (a BuzzSynth, heard from where it is:
// higher and louder when it dives). Its behaviour (enum Behavior):
//
//   Wander --(it senses water)--> ToWater --(over it)--> Lay --(LAY_TIME)--> Wander;
//   at dawn and at dusk (isAttackTime), Wander / ToWater / Lay --(the target comes within
//   DETECT_RANGE)--> Stalk --(every few seconds)--> Dive --(it bit the player / it pulled up in
//   front of the vehicle / it missed)--> Retreat --> Stalk;  Stalk --(the target gets further
//   than LOSE_RANGE, the player is dead, or it is no longer dawn or dusk)--> Wander;
//   from any state, (a collision flings it: the RV hits it) --> Dead.
//
//  - Wander: it roams the desert looking for water to lay its eggs in, flying slowly from one
//    random point to another within WATER_ROAM_RADIUS of its home (setHome).
//  - ToWater: a pool of water (setWaterSpots) is within WATER_SENSE and it has not laid in it for
//    LAY_COOLDOWN seconds: it flies to it.
//  - Lay: it hovers low over the water, its abdomen down, for LAY_TIME seconds, and lays its
//    eggs (it only counts them: getEggs).
//  - Stalk: it circles round the target (the playable character: setTarget) at STALK_RADIUS and
//    STALK_HEIGHT above the ground, facing it, bobbing; after a random wait it dives. Just before,
//    its legs open into a claw pointing at the player.
//  - Dive: it shoots at the target's head with all six legs pointing at it in the shape of a claw
//    (a ring of feet round the head that closes as it gets there). On foot, if it gets within
//    BITE_RANGE of it, it bites: the stage's callback (setPlayerCaughtCallback: the player dies).
//    In the vehicle it does not dare to touch it: it pulls up VEHICLE_PULL_UP metres from it.
//  - Retreat: it climbs away from the target for RETREAT_TIME seconds.
//  - Siphon (any time of day; setVehicle): the player is neither in the vehicle nor within
//    FUEL_SAFE_DISTANCE of it, and the vehicle has fuel and is within VEHICLE_SENSE: it flies to
//    its fuel cap, clings to the wall with its legs, puts the tip of its proboscis in the cap
//    and sucks SIPHON_RATE of the tank per second. If the player comes back (or gets in, or the
//    tank is empty) it leaves (Retreat).
//  - TireAttack: while the player drives the vehicle, every TIRE_CHECK_INTERVAL seconds there is
//    a TIRE_ATTACK_CHANCE that it goes for one of its wheels, chosen at random: it chases it with
//    its legs reaching for it and bursts the tyre when it gets within TIRE_REACH
//    (RV::punctureTire: the vehicle pulls towards that side), then leaves (Retreat). It gives
//    up after TIRE_TIMEOUT.
//  Priority: laying its eggs (an adult with blood that senses water: nothing interrupts it) >
//  attacking the player (at dawn and dusk) > the tyres > the fuel > roaming.
//  - Dead: it falls, rolls onto its back and lies there with its legs curled up, silent (it stays
//    in the stage as a GameObject). A blow that changes its velocity by more than
//    DEATH_SPEED_CHANGE kills it (not its own flight stopped against something).
// Its stomach holds fuel and blood apart. Blood: an adult gets it by biting the player (which kills
// him) and needs it to lay its eggs; a young one gets it by biting the player (Feed: it flies to his
// head; Bite: it clings there with its legs, pumping its proboscis, its abdomen filling, for
// BITE_TIME, without killing him) and grows only with it. Full of fuel, an adult that dives at the
// player blows up next to him instead of biting (the blast kills him).
// It only attacks at dawn and at dusk (the hour from setHourQuery; without it, always). It never
// flies lower than MIN_CLEARANCE above the ground (its legs hang below its position).
//
// Eggs: while it lays (Lay) it drops its eggs one by one on the water through setEggLayer (the
// stage makes each one a MosquitoEgg, which hatches into a new Mosquito). A young one is born at
// BABY_SCALE of the size and grows to full size by drinking blood (setGrowth: the object's scale);
// until then it flutters round where it was born and bites the player (no fuel, no tyres, no
// laying, and its bites do not kill).
//
// Its stomach: the fuel it sucks fills it (STOMACH_CAPACITY of a tank fills it up) and its abdomen
// (a part of its own) swells with it; full, it leaves. It does not digest fuel: full, it stays full
// (and dives at the player as a bomb: KAMIKAZE_FUEL) until it blows up.
//
// Bursting a tyre blows it up (explode): its legs, wings and body fly apart and fall (debris with
// their own physics), its abdomen bursts in a splash, there is a ball of fire and smoke (three
// particle emitters: getEmitters, for the stage), a flash of light (getLight) and a bang. It is
// dead then (Dead), its pieces lying where they fell.
class Mosquito : public DynamicGameObject {
public:
  // The places round the player's head where young ones bite, shared by all the mosquitoes of a
  // stage (setBiteSlots) so that several can bite him at once, each in its own: SLOTS directions
  // round the head, at two heights in turn (cheek and temple). Each holds the one biting there.
  struct BiteSlots {
    static const int SLOTS = 8;
    const Mosquito *owner[SLOTS] = {nullptr, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr};
  };

  enum class Behavior { Wander, ToWater, Lay, Siphon, TireAttack, Feed, Bite, Stalk, Dive, Retreat, Dead };

  // Blood: it needs at least BLOOD_TO_LAY (of a full stomach) to lay its eggs (laying uses it up).
  // A young one only grows by drinking it: BLOOD_GROWTH_TIME seconds of biting make it full size.
  // It goes for the player on foot within YOUNG_DETECT (m), at any hour, bites him (it does not
  // kill) for BITE_TIME (s), and waits FEED_COOLDOWN (s) before the next bite.
  static constexpr float BLOOD_TO_LAY = 0.5f;
  static constexpr float BLOOD_GROWTH_TIME = 30.0f;
  static constexpr float YOUNG_DETECT = 25.0f;
  static constexpr float BITE_TIME = 5.0f;
  static constexpr float FEED_COOLDOWN = 8.0f;
  static constexpr float YOUNG_DIGEST_TIME = 60.0f; // (a young one turns its blood into growth)
  // Full of fuel (at least this share of its stomach) its dive at the player does not end in a bite:
  // it blows up next to him, and the blast kills the player on foot within BLAST_RADIUS (m)
  static constexpr float KAMIKAZE_FUEL = 0.9f;
  static constexpr float BLAST_RADIUS = 6.0f;
  // Biting, it holds its head down this much (radians)
  static constexpr float BITE_PITCH = 0.35f;

  // The fuel: the player must be at least this far (m) from the vehicle, which must be within
  // VEHICLE_SENSE (m) of it; it sucks this share of the tank per second (1 %), once its spot by
  // the cap is within SIPHON_REACH (m)
  static constexpr float FUEL_SAFE_DISTANCE = 20.0f;
  static constexpr float VEHICLE_SENSE = 80.0f;
  static constexpr float SIPHON_RATE = 0.01f;
  static constexpr float SIPHON_REACH = 0.8f;
  // The tyres: how often (s) it thinks of going for one while the player drives, the chance each
  // time (5 %), how near it must get to the wheel's hub (m) and when it gives up (s)
  static constexpr float TIRE_CHECK_INTERVAL = 10.0f;
  static constexpr float TIRE_ATTACK_CHANCE = 0.05f;
  static constexpr float TIRE_REACH = 2.8f;
  static constexpr float TIRE_TIMEOUT = 8.0f;
  static constexpr float TIRE_SENSE = 60.0f; // the vehicle must be this near (m) to try
  static constexpr float VEHICLE_ROOF = 3.3f; // how tall the vehicle is (m): it flies over it
  // The tip of its proboscis, in its body's frame (generate_mosquito.py)
  static constexpr float PROBOSCIS_TIP_Y = -1.05f, PROBOSCIS_TIP_Z = 1.70f;

  // Flying speeds (m/s): wandering and circling, and diving
  static constexpr float FLY_SPEED = 6.0f;
  static constexpr float DIVE_SPEED = 13.0f;
  // It notices the target within this distance (m) and forgets it beyond the second one
  static constexpr float DETECT_RANGE = 35.0f;
  static constexpr float LOSE_RANGE = 55.0f;
  // How it circles round the target: radius (m), height over the ground (m) and angular speed (rad/s)
  static constexpr float STALK_RADIUS = 9.0f;
  static constexpr float STALK_HEIGHT = 4.5f;
  static constexpr float STALK_TURN_RATE = 0.45f;
  // The time it circles before each dive (s), random between these two
  static constexpr float DIVE_WAIT_MIN = 2.5f, DIVE_WAIT_MAX = 6.0f;
  // A dive gives up after this long (s); after it, it climbs away for RETREAT_TIME (s)
  static constexpr float DIVE_TIMEOUT = 2.5f;
  static constexpr float RETREAT_TIME = 2.0f;
  // It bites the player on foot when its body is this close to his head (m): the proboscis is
  // 1.7 m long, the tip reaches the head from there
  static constexpr float BITE_RANGE = 2.0f;
  // The head of the player above his feet (m), what it aims at
  static constexpr float HEAD_HEIGHT = 1.6f;
  // In the vehicle it pulls up this far (m) from the point it aims at (above the vehicle's position)
  static constexpr float VEHICLE_PULL_UP = 4.5f;
  static constexpr float VEHICLE_AIM_HEIGHT = 2.2f;
  // It attacks only between these hours (0-24): dawn and dusk (the day map's twilight is 5-6 and
  // 18-19; see ARCHITECTURE.md, "Hora del día")
  static constexpr float DAWN_FROM = 5.0f, DAWN_TO = 7.0f;
  static constexpr float DUSK_FROM = 17.5f, DUSK_TO = 19.5f;
  // Roaming in search of water: how far from home (m) and how high over the ground (m); it senses
  // water this far away (m)
  static constexpr float WATER_ROAM_RADIUS = 55.0f;
  static constexpr float WANDER_HEIGHT_MIN = 3.5f, WANDER_HEIGHT_MAX = 7.0f;
  static constexpr float WATER_SENSE = 30.0f;
  // Laying: how long (s), how high it hovers over the water (m), how many eggs (random between
  // these two) and how long until it lays in the same water again (s)
  static constexpr float LAY_TIME = 8.0f;
  static constexpr float LAY_HEIGHT = 2.1f;
  static constexpr int EGGS_MIN = 3, EGGS_MAX = 5;
  static constexpr float LAY_COOLDOWN = 90.0f;
  // The claw: its legs start to open this long (s) before it dives, and how fast they go into it and
  // come out of it (1/s); the ring of its feet round the target is CLAW_OPEN wide (m) far from
  // it and CLAW_CLOSED when it gets there; it reaches between these two distances (m) from its hips
  static constexpr float CLAW_PREPARE = 1.0f;
  static constexpr float CLAW_IN_RATE = 2.5f, CLAW_OUT_RATE = 1.2f;
  static constexpr float CLAW_OPEN = 0.65f, CLAW_CLOSED = 0.12f;
  static constexpr float CLAW_REACH_MIN = 1.3f, CLAW_REACH_MAX = 2.3f;
  // Never lower than this over the ground (m): its feet are 1.7 m below its position
  static constexpr float MIN_CLEARANCE = 1.9f;
  // A collision that changes its velocity by more than this (m/s) kills it
  static constexpr float DEATH_SPEED_CHANGE = 5.0f;
  // Dead, it lies on its back with its position this high over the ground (m)
  static constexpr float DEAD_REST_HEIGHT = 0.35f;
  // Growing: the size it is born at (of the adult's) and how long it takes to be full size (s);
  // young, it flutters within JUVENILE_ROAM (m) of where it was born
  static constexpr float BABY_SCALE = 0.05f;
  static constexpr float JUVENILE_ROAM = 8.0f;
  // The stomach: a full one holds this share of a tank (it never empties). Full, the abdomen is (1 + ABDOMEN_SWELL_*) times as
  // thick and long, about where it joins the thorax (generate_mosquito.py, ABDOMEN_PIVOT)
  static constexpr float STOMACH_CAPACITY = 0.125f;
  static constexpr float ABDOMEN_SWELL_XY = 0.9f, ABDOMEN_SWELL_Z = 0.35f;
  static constexpr float ABDOMEN_PIVOT_Y = -0.05f, ABDOMEN_PIVOT_Z = -0.30f;
  // The explosion: how long the flash lasts (s), how far it lights (m), how hard the pieces fly
  // (m/s) and how loud the bang is
  static constexpr float FLASH_TIME = 0.6f;
  static constexpr float FLASH_RANGE = 16.0f;
  static constexpr float DEBRIS_SPEED_MIN = 4.0f, DEBRIS_SPEED_MAX = 10.0f;
  static constexpr float BANG_VOLUME = 2.5f;
  // The wings (see generate_mosquito.py, WING_HINGE): the left hinge on the body (the right one
  // is its mirror image), how often they beat on screen (Hz: the real beat is far faster, this
  // reads as a blur), how far (degrees, up and down about a raised middle) and how far they
  // sweep forward and back
  static constexpr float WING_HINGE_X = 0.20f, WING_HINGE_Y = 0.27f, WING_HINGE_Z = 0.10f;
  static constexpr float WING_BEAT_HZ = 15.0f;
  static constexpr float WING_FLAP_DEG = 48.0f, WING_RAISE_DEG = 12.0f, WING_SWEEP_DEG = 22.0f;
  // The buzz: the wingbeat it is heard at (Hz), hovering and at full speed, and its volume
  static constexpr float BUZZ_HZ = 165.0f, BUZZ_DIVE_HZ = 235.0f;
  static constexpr float BUZZ_VOLUME = 1.6f;

private:
  Behavior behavior = Behavior::Wander;
  float health = 1.0f; // (shots: takeDamage)
  float stateTime = 0.0f;  // seconds in the current behaviour
  float diveWait = 0.0f;   // seconds left circling before the next dive
  float circleAngle = 0.0f; // where it is on the circle round the target (radians)
  float circleDirection = 1.0f;
  glm::vec3 home = glm::vec3(0.0f);
  std::vector<glm::vec3> waterSpots;
  std::vector<float> waterCooldown; // seconds until it lays in each again
  int water = -1;                   // the one it goes to / lays in
  int eggs = 0;
  std::function<float()> hourQuery;
  RV *vehicle = nullptr;
  int tireTarget = 0;          // the wheel it goes for (TireAttack)
  float tireCheck = 0.0f;      // seconds until it next thinks of the tyres
  bool sucking = false;        // (Siphon) its proboscis is in the cap
  float fuelTaken = 0.0f;      // share of a tank it has sucked, all in all
  bool wantsFuel() const;
  void siphonSpot(glm::vec3 &spot, float &faceYaw) const;
  // The legs: their physics, their parts (leg * 4 + segment), how far they are in the claw (0..1)
  // and curled up (dead, 0..1)
  InsectLegs legs;
  std::vector<size_t> legParts;
  float clawWeight = 0.0f, curlWeight = 0.0f;
  size_t abdomenPart = 0;
  float growth = 1.0f;         // 0 = just hatched .. 1 = full size
  float stomach = 0.0f;        // 0 = empty .. 1 = full of fuel
  float blood = 0.0f;          // 0 = none .. 1 = full of blood
  float feedCooldown = 0.0f;   // (young) seconds until it bites again
  glm::vec3 biteDir = glm::vec3(1.0f, 0.0f, 0.0f); // (Feed, Bite) from the head out to it
  float biteHeight = -0.12f;                       // ...and how far up from the eyes
  std::shared_ptr<BiteSlots> biteSlots;
  int biteSlot = -1;                               // the one it holds, or -1
  bool claimBiteSlot(const glm::vec3 &head);
  void releaseBiteSlot();
  glm::vec3 biteSpot(const glm::vec3 &head, float pump) const;
  void updateBite(double dt);
  float swell = 0.0f;          // how swollen the abdomen is now (follows the stomach)
  std::function<bool(const glm::vec3 &)> eggLayer;
  int eggsToLay = 0;           // (Lay) still to drop this time
  float nextEgg = 0.0f;        // (Lay) when the next one drops
  // The explosion: each part flying on its own (in the world), the emitters and the flash
  struct Debris {
    size_t part;
    glm::mat4 world;    // where it is now
    glm::vec3 centre;   // its middle, in the part's frame (what it spins about)
    glm::vec3 velocity;
    glm::vec3 axis;
    float spin;         // rad/s
    bool resting;
  };
  bool exploded = false;
  std::vector<Debris> debris;
  glm::mat4 frozen = glm::mat4(1.0f); // the object's frame when it blew up
  std::vector<glm::vec3> partCentres; // the middle of each part, in the body's frame
  std::vector<std::shared_ptr<ParticleEmitter>> emitters; // fire, smoke, splash
  float flashTime = 0.0f;
  std::unique_ptr<Sound> bangSound;
  void makeEmitters();
  void updateDebris(double dt);
  void updateAbdomen(double dt);
  glm::vec3 waypoint = glm::vec3(0.0f);
  bool hasWaypoint = false;
  float yaw = 0.0f, pitch = 0.0f, roll = 0.0f; // the body's attitude (radians)
  float deadRoll = 0.0f;                       // how far it has rolled over, dead (0..pi)
  bool landed = false;                         // dead, it has reached the ground
  float lastDeadY = 0.0f;                      // (dead: its height last frame)
  float wingPhase = 0.0f;
  size_t wingLeft = 0, wingRight = 0;          // their parts
  std::mt19937 random;

  std::function<glm::vec3()> targetPosition;
  std::function<bool()> playerInVehicle;
  std::function<void()> playerCaught;
  std::function<bool()> playerDead;
  std::function<bool(float, float, float &)> floorHeight;

  SoundEngine &soundEngine;
  std::shared_ptr<BuzzSynth> buzz;
  std::unique_ptr<Sound> buzzSound;

  float uniform(float a, float b) { return std::uniform_real_distribution<float>(a, b)(random); }
  float groundAt(float x, float z) const;
  void enterBehavior(Behavior next);
  glm::vec3 aimPoint() const;
  glm::vec3 wanderVelocity();
  int senseWater() const;
  void updateLegs(double dt);
  // The legs reach for `target` (world), the ring of their feet `ring` metres wide
  void clawPose(InsectLegs::Pose &pose, const glm::vec3 &target, float ring) const;
  void curlPose(InsectLegs::Pose &pose) const;
  glm::vec3 stalkVelocity(const glm::vec3 &target, double dt);
  void updateAttitude(double dt);
  void updateWings(double dt);
  void updateBuzz();
  void updateDead(double dt);
  void updateReplica(double dt);
  const char *behaviorName() const;

public:
  // The legs' joints (left legs, front to back: hip, ..., foot; the right legs are their mirror
  // images), in the body's frame, as generate_mosquito.py makes them (LEGS)
  static const float LEG_JOINTS[3][5][3];

  // `legSegments`: the 24 models of the legs' segments, leg by leg (L0, R0, L1, R1, L2, R2: pair 0
  // is the front one) and segment by segment from the hip (mosquito_leg_<L|R><pair>_<segment>.obj)
  // `body` is the head and thorax, `abdomen` its abdomen (mosquito_abdomen.obj)
  Mosquito(std::shared_ptr<Model> body, std::shared_ptr<Model> abdomen, std::shared_ptr<Model> wingLeft,
           std::shared_ptr<Model> wingRight, const std::vector<std::shared_ptr<Model>> &legSegments,
           SoundEngine &engine);

  // Who it goes for: the feet of the character the player controls (asked every frame)
  void setTarget(std::function<glm::vec3()> where) { targetPosition = where; }
  // Whether the player is inside the vehicle (then it does not touch him); without it, never
  void setPlayerInVehicleQuery(std::function<bool()> inVehicle) { playerInVehicle = inVehicle; }
  // What it does when it bites the player on foot (the stage kills him), and whether he is dead
  // (then it leaves him); without them it only dives at him
  void setPlayerCaughtCallback(std::function<void()> caught) { playerCaught = caught; }
  void setPlayerDeadQuery(std::function<bool()> dead) { playerDead = dead; }
  // The height of the ground at (x, z); false where there is none
  void setFloorQuery(std::function<bool(float, float, float &)> floor) { floorHeight = floor; }
  // The point it roams round
  void setHome(const glm::vec3 &where) { home = where; }
  // The pools of water it can lay its eggs in (their middle, on the ground)
  void setWaterSpots(const std::vector<glm::vec3> &spots) {
    waterSpots = spots;
    waterCooldown.assign(spots.size(), 0.0f);
  }
  // The hour of the day (0-24, asked every frame): it attacks only at dawn and at dusk
  void setHourQuery(std::function<float()> hour) { hourQuery = hour; }
  bool isAttackTime() const;
  int getEggs() const { return eggs; }
  // The vehicle whose fuel it sucks and whose tyres it bursts (the stage keeps it alive as long
  // as the mosquito); without it, neither
  void setVehicle(RV *rv) { vehicle = rv; }
  float getFuelTaken() const { return fuelTaken; }
  // Drops one egg at that point of the water (the stage makes it; false: no more, it stops laying)
  void setEggLayer(std::function<bool(const glm::vec3 &)> layer) { eggLayer = layer; }
  // How grown it is (0 = just hatched, BABY_SCALE of the size .. 1 = full size)
  void setGrowth(float g);
  float getGrowth() const { return growth; }
  bool isGrown() const { return growth >= 1.0f; }
  float getStomach() const { return stomach; }
  void setStomach(float fill) { stomach = glm::clamp(fill, 0.0f, 1.0f); }
  // The bite places round the player's head, shared with the other mosquitoes (without them,
  // each bites on the side it comes from, maybe where another one is)
  void setBiteSlots(std::shared_ptr<BiteSlots> slots) { biteSlots = slots; }
  ~Mosquito() override { releaseBiteSlot(); }
  float getBlood() const { return blood; }
  void setBlood(float fill) { blood = glm::clamp(fill, 0.0f, 1.0f); }
  // Blows it up (see the class comment)
  void explode();
  bool hasExploded() const { return exploded; }
  // Its fire, smoke and splash: give them to the stage (Stage::addEmitter)
  const std::vector<std::shared_ptr<ParticleEmitter>> &getEmitters() const { return emitters; }
  // The flash of its explosion, while it lasts
  void getLight(std::vector<SpotLight> &lights) const;

  Behavior getBehavior() const { return behavior; }
  bool isDead() const { return behavior == Behavior::Dead; }
  void kill();

  void update(double dt) override;
  unsigned char netKind() const override { return NET_MOSQUITO; }
  float netSpawnArg() const override { return growth; }
  // What the clients need to show it: what it does, how grown and full it is, and the bite's place
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  void applyCollision(const glm::vec3 &push, const glm::vec3 &velocityChange) override;
  // Shot: once its health is gone it dies (kill)
  void takeDamage(float amount, const glm::vec3 &direction, const Stage &stage) override;
  void teleport(const glm::vec3 &position) override;
  float getHeading() const override { return yaw; }
  void turn(float radians) override;
  void describe(std::vector<std::string> &lines) const override;
  void getProperties(std::vector<Property> &properties) override;
};

#endif
