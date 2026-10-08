#ifndef SAUCER
#define SAUCER

#include <functional>
#include <memory>
#include <random>

#include "Interactable.h"
#include "ParticleEmitter.h"
#include "PlayableCharacter.h"
#include "SoundEngine.h"
#include "SpotLight.h"

class Bob;
class Camera;

// Bob's ship (assets/bob/saucer_*.obj, generate_saucer.py): a flying saucer that comes for the
// night. On its own it follows a sequence (enum Phase), driven by the clock:
//
//   Away (not there) --(night falls)--> Arriving (it comes over the trees, spinning, from far
//   and high) --> Descending (a beam of light under it; it slows to the ground and its legs come
//   out) --> Landed (it stands and waits) --(Bob is back on board: boarded())--> Closing -->
//   Ascending (straight up, legs in, the beam on) --> Leaving (off and away) --> Away.
//
// While it stands (Landed) its ramp comes down when Bob is near it (inside, coming out, going in,
// or within RAMP_NEAR of its foot) and goes up again when he walks away. With the ramp down the
// player can get in (an Interactable: the Use key at the foot of the ramp: the map's enter action).
//
// Then he flies it (Piloted; it is a PlayableCharacter): the engine key starts it (and stops it),
// the movement keys move it (the way the camera looks), Space lifts it and Shift brings it down;
// the ship-legs key draws its legs in or puts them out while it is in the air. Without them it
// can come down to its belly. Only standing on the ground on its legs can the player get out
// (canDisembark: Shift then, the map's leave action). With the engine off it sinks slowly to the
// ground. The camera follows it from behind and above.
//
// Its parts: the hull, its lights (glowing), the legs (they slide up into the hull), the ramp (it
// turns about its hinge) and the beam (a translucent cone, glowing). Its frame: the origin is the
// ground under its middle when it stands on its legs, +z is where its ramp opens (setLanding).
// It moves itself (contactFloor) and only collides while it stands or the player flies it.
//
// Its sounds (setSounds, from its middle): a hum while it moves (on its own, or flown with the
// engine on: higher the faster), which dies away as it comes down to land while it powers down
// (saucer_power_down.wav, the length of the descent); and when it touches down on its legs (on its
// own or flown), a release of steam, with smoke from its feet and from round its ramp (getEmitters:
// the map draws those emitters).
//
// Its ray gun (flying it, the headlights key: toggleGun): it comes down from under its middle and the
// camera goes to it, in first person: the mouse aims it (the gun turns with the view) and fire()
// (the main loop: the left button) shoots a green ray straight ahead, at most every SHOT_COOLDOWN:
// the first thing it meets (a solid object, or the ground) within SHOT_RANGE takes SHOT_DAMAGE
// (GameObject::takeDamage), with sparks and a flash where it hits, and a sound
// (saucer_shot.wav). The same key puts it away, and the camera goes back behind the ship.
class Saucer : public PlayableCharacter, public Interactable {
public:
  enum class Phase { Away, Arriving, Descending, Landed, Piloted, Closing, Ascending, Leaving };

  // Its measures (generate_saucer.py): the legs' height, the ramp's hinge and length
  static constexpr float LEG_HEIGHT = 1.6f;
  static constexpr float RAMP_HINGE_Y = 1.68f, RAMP_HINGE_Z = 0.6f, RAMP_LENGTH = 2.9f;
  static constexpr float RADIUS = 4.5f;
  // How long each phase lasts (s), and where it comes from: so far away and so high (m)
  static constexpr float ARRIVE_TIME = 10.0f, DESCEND_TIME = 6.0f, RAMP_TIME = 2.0f;
  static constexpr float ASCEND_TIME = 4.0f, LEAVE_TIME = 7.0f;
  static constexpr float FAR_AWAY = 160.0f, HIGH_UP = 90.0f, HOVER_HEIGHT = 14.0f;
  static constexpr float SPIN_RATE = 1.6f; // rad/s, while it flies on its own
  // The ramp comes down while Bob is this near its foot (m)
  static constexpr float RAMP_NEAR = 7.0f;
  // Flying it: top speed across (m/s), up and down (m/s), how fast it gets there (1/s), how fast
  // it sinks with the engine off (m/s), how high it can go over the ground (m), how long the legs
  // take to go in or out (s)
  static constexpr float FLY_SPEED = 18.0f, CLIMB_SPEED = 8.0f, RESPONSE = 2.0f;
  static constexpr float SINK_SPEED = 3.0f, CEILING = 120.0f, LEGS_TIME = 1.2f;
  // Its sounds: how loud (1 = as recorded), and how near (m) they are heard at full volume (they
  // fade with distance from there)
  static constexpr float HUM_VOLUME = 1.6f, POWER_DOWN_VOLUME = 0.7f, STEAM_VOLUME = 1.6f;
  static constexpr float SOUND_NEAR = 8.0f;
  // The smoke when it touches down: for how long (s), and how much at first (puffs per second
  // from each of its six vents: three feet, three round the ramp)
  static constexpr float SMOKE_TIME = 3.0f, SMOKE_RATE = 35.0f;
  // The ray gun: its pivot (the eye when aiming) over the ground under the middle, how far up it
  // slides when put away (m), how long it takes (s); the shot: how far it goes (m), how often (s),
  // how long it shows (s), how much it hurts (of a creature's health), how loud it is
  static constexpr float GUN_PIVOT_Y = LEG_HEIGHT - 0.5f, GUN_TRAVEL = 1.1f, GUN_TIME = 0.6f;
  static constexpr float SHOT_RANGE = 150.0f, SHOT_COOLDOWN = 0.3f, SHOT_SHOW = 0.12f;
  static constexpr float SHOT_DAMAGE = 0.5f, SHOT_VOLUME = 0.7f;
  // The camera, flying it: distance and height
  static constexpr float CAMERA_DISTANCE = 18.0f, CAMERA_HEIGHT = 6.0f;

private:
  Phase phase = Phase::Away;
  float phaseTime = 0.0f;
  glm::vec3 landing = glm::vec3(0.0f);
  float landingYaw = 0.0f;
  glm::vec3 from = glm::vec3(0.0f), to = glm::vec3(0.0f); // the flight of the current phase
  float spin = 0.0f;           // its turn about its axis now (radians)
  float spinFrom = 0.0f;       // (Descending: where the turn starts settling from)
  float legsOut = 0.0f;        // 0 = in the hull .. 1 = standing on them
  bool legsWanted = true;      // (Piloted) out or in, as the player asked
  float rampOpen = 0.0f;       // 0 = closed .. 1 = down on the ground
  bool bobOut = false;         // (Landed) Bob has been let out this visit
  bool visitedTonight = false; // (it comes once a night)
  bool taking = false;         // it is taking the player up its beam
  // flying it
  bool engineOn = false;
  bool onGround = true;
  glm::vec2 steer = glm::vec2(0.0f); // the movement keys, turned to the camera's heading
  float lift = 0.0f;                 // up (+1) or down (-1)
  Camera *camera = nullptr;
  std::shared_ptr<Bob> bob;
  std::function<bool()> isNight;
  std::function<void()> enterAction;
  size_t hullPart = 0, lightsPart = 0, legsPart = 0, rampPart = 0, beamPart = 0;
  size_t gunMountPart = 0, gunPart = 0, shotPart = 0;
  // the ray gun
  bool gunWanted = false;  // (Piloted) out, as the player asked
  float gunOut = 0.0f;     // 0 = in the hull .. 1 = down, ready
  bool aiming = false;     // the camera is at the gun
  glm::mat4 gunLocal = glm::mat4(1.0f); // the barrel's placement in its frame
  float shotCooldown = 0.0f;
  float shotTime = -1.0f;  // seconds since the last shot (< 0: none showing)
  glm::vec3 shotFrom = glm::vec3(0.0f), shotTo = glm::vec3(0.0f); // the muzzle and where it hit, when
                                                                  // it was fired (world: it stays put)
  std::shared_ptr<AudioClip> shotClip;
  std::unique_ptr<Sound> shotSound;
  std::shared_ptr<ParticleEmitter> sparks;
  std::vector<std::shared_ptr<ParticleEmitter>> emitters; // (smoke and sparks)
  std::mt19937 random;
  // the server has no camera: where the pilot looks comes from his controls (setAim)
  glm::vec3 aimForward = glm::vec3(0.0f, 0.0f, 1.0f);
  // network (see writeNetState): how many times it has touched down and fired, so that a client
  // does the smoke and the sparks of each once; what the server says the parts are doing now
  unsigned touchdowns = 0, shots = 0;
  bool netPrimed = false;
  float legsOutWanted = 0.0f, rampOpenWanted = 0.0f, gunOutWanted = 0.0f;
  float gunYaw = 0.0f, gunPitch = 0.0f;
  void updateReplica(double dt);
  void setBarrel(float yawL, float pitchL);
  // sounds and smoke
  SoundEngine *soundEngine = nullptr;
  std::shared_ptr<AudioClip> humClip, powerDownClip, steamClip;
  std::unique_ptr<Sound> hum, powerDown, steam;
  float humLevel = 0.0f;  // 0..1, how loud the hum is now
  float smokeTime = -1.0f; // seconds since it touched down (< 0: no smoke)
  std::vector<std::shared_ptr<ParticleEmitter>> smoke;

  void enter(Phase next);
  void touchDown();
  void updateSounds(double dt);
  void updateSmoke(double dt);
  void updateGun(double dt);
  glm::vec3 muzzle() const; // (world)
  void place(); // its turn and parts for what it is doing
  bool beamOn() const;
  bool bobNearRamp() const;
  void fly(const class Stage &stage, double dt);

public:
  Saucer(std::shared_ptr<Model> hull, std::shared_ptr<Model> lights, std::shared_ptr<Model> legs,
         std::shared_ptr<Model> ramp, std::shared_ptr<Model> beam, std::shared_ptr<Model> gunMount,
         std::shared_ptr<Model> gun, std::shared_ptr<Model> shot);

  // Where it lands (the ground under its middle) and which way its ramp opens (radians about +y,
  // 0 = +z)
  void setLanding(const glm::vec3 &spot, float yaw);
  void setNightQuery(std::function<bool()> night) { isNight = night; }
  // (the server) where the pilot looks, which the ray gun follows
  void setAim(const glm::vec3 &forward) { aimForward = forward; }
  void setBob(std::shared_ptr<Bob> alien) { bob = alien; }
  // What using its ramp does (the map puts the player in it)
  void setEnterAction(std::function<void()> action) { enterAction = action; }
  // Its sounds (assets/bob), played on `engine`; without this it is silent
  void setSounds(SoundEngine &engine);
  // Its smoke and the sparks of its shots: the map adds these emitters to its own
  // (Stage::addEmitter)
  const std::vector<std::shared_ptr<ParticleEmitter>> &getEmitters() const { return emitters; }

  Phase getPhase() const { return phase; }
  // It stands with its ramp all the way down
  bool isRampDown() const { return phase == Phase::Landed && rampOpen >= 0.99f; }
  // The top of the ramp (at its hinge, under the hull) and its foot, on the ground (world)
  glm::vec3 rampTop() const;
  glm::vec3 rampFoot() const;
  // Bob is back inside: it closes and goes
  void boarded();
  // It is taking the player: its beam stays on until it has gone
  void takePlayer() { taking = true; }
  // Where the player is taken to, and where he rides while he flies it: the middle of its
  // underside (world)
  glm::vec3 hatch() const;
  // Where the ray gun's pivot is (the pilot's eye while he aims), in the world
  glm::vec3 gunPivot() const { return position + glm::vec3(rotation * glm::vec4(0.0f, GUN_PIVOT_Y, 0.0f, 0.0f)); }

  // Flying it: the player gets in (it stands, its ramp down) or out
  void setPiloted(bool piloted);
  bool isPiloted() const { return phase == Phase::Piloted; }
  // He can get out: it stands on the ground on its legs
  bool canDisembark() const { return phase == Phase::Piloted && onGround && legsOut >= 0.99f; }
  void toggleEngine();
  bool isEngineOn() const { return engineOn; }
  void toggleLegs();
  // (Piloted) the ray gun comes down and the camera goes to it, or it goes back up
  void toggleGun();
  bool isAiming() const { return aiming; }
  // (Piloted, aiming) shoots along `direction` from `eye` (the camera): false if it can't now
  bool fire(const class Stage &stage, const glm::vec3 &eye, const glm::vec3 &direction);

  // PlayableCharacter
  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  void control(glm::vec2 dir, float up, float cameraYaw) override;

  // Interactable: get in at the foot of the ramp
  std::string getInteractionName() const override { return "nave de Bob"; }
  std::string getInteractionVerb() const override { return "entrar en la"; }
  glm::vec3 getInteractionPoint() const override { return rampFoot(); }
  float getInteractionRange() const override { return 2.5f; }
  bool isInteractionAvailable() const override;
  bool usesDirectly() const override { return true; }
  void onUse(const glm::vec3 &playerPosition) override {
    if (enterAction)
      enterAction();
  }
  void buildInterface(UIPanel &panel) override {}

  void update(double dt) override;
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  bool contactFloor(const Stage &stage, double dt) override;
  void applyCollision(const glm::vec3 &push, const glm::vec3 &velocityChange) override;
  // Its beam (a spot light pointing down) while it lands, takes off or takes the player, and a
  // faint glow under it while it stands or flies with the player
  void getLights(std::vector<SpotLight> &lights) const;
  void describe(std::vector<std::string> &lines) const override;
  void getProperties(std::vector<Property> &properties) override;
};

#endif
