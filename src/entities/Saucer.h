#ifndef SAUCER
#define SAUCER

#include <functional>
#include <memory>
#include <random>

#include "Interactable.h"
#include "PlayableCharacter.h"
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
  std::mt19937 random;

  void enter(Phase next);
  void place(); // its turn and parts for what it is doing
  bool beamOn() const;
  bool bobNearRamp() const;
  void fly(const class Stage &stage, double dt);

public:
  Saucer(std::shared_ptr<Model> hull, std::shared_ptr<Model> lights, std::shared_ptr<Model> legs,
         std::shared_ptr<Model> ramp, std::shared_ptr<Model> beam);

  // Where it lands (the ground under its middle) and which way its ramp opens (radians about +y,
  // 0 = +z)
  void setLanding(const glm::vec3 &spot, float yaw);
  void setNightQuery(std::function<bool()> night) { isNight = night; }
  void setBob(std::shared_ptr<Bob> alien) { bob = alien; }
  // What using its ramp does (the map puts the player in it)
  void setEnterAction(std::function<void()> action) { enterAction = action; }

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

  // Flying it: the player gets in (it stands, its ramp down) or out
  void setPiloted(bool piloted);
  bool isPiloted() const { return phase == Phase::Piloted; }
  // He can get out: it stands on the ground on its legs
  bool canDisembark() const { return phase == Phase::Piloted && onGround && legsOut >= 0.99f; }
  void toggleEngine();
  bool isEngineOn() const { return engineOn; }
  void toggleLegs();

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
  bool contactFloor(const Stage &stage, double dt) override;
  void applyCollision(const glm::vec3 &push, const glm::vec3 &velocityChange) override;
  // Its beam (a spot light pointing down) while it lands, takes off or takes the player, and a
  // faint glow under it while it stands or flies with the player
  void getLights(std::vector<SpotLight> &lights) const;
  void describe(std::vector<std::string> &lines) const override;
  void getProperties(std::vector<Property> &properties) override;
};

#endif
