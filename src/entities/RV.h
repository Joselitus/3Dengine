#ifndef RV_CHARACTER
#define RV_CHARACTER

#include <functional>
#include <memory>
#include <random>

#include "Camera.h"
#include "Interactable.h"
#include "EngineSimulator.h"
#include "EngineSound.h"
#include "ImpactDetector.h"
#include "ParticleEmitter.h"
#include "MirrorView.h"
#include "PlayableCharacter.h"
#include "SoundEngine.h"
#include "SpotLight.h"
#include "VehicleBody.h"

// The RV is HOLLOW: its collision shape (a CompoundShape, see hullShape in RV.cpp) is a floor, a roof
// and walls with the doorway cut in the driver's side (+x), so the penguin can walk in (the sill and
// the step under it are low enough to climb: Walker::getStepHeight) and around the cab. The door is a
// model of its own that swings on its front edge (setDoorModel); E next to it gives it a push to open
// or to shut (isDoorOpen, toggleDoor; shut, it is a wall too). The door is a real hinged panel: it
// feels the vehicle's acceleration and spin (and gravity when the RV is tilted), so it swings open
// when the RV brakes or turns hard, and slams shut when it accelerates; it bounces off its stops
// and latches when it closes gently. To drive, the penguin stands by the steering
// wheel inside and presses E there (steeringInteraction()): that is what the enter action does.
//
// The RV: W/S drive it and A/D steer its front wheels, so the camera can
// orbit freely without turning the mesh. It is a VehicleBody: a rigid chassis
// on four springs, so it bounces, pitches and rolls over the terrain, and its
// wheels move up and down with the suspension. The camera follows it from
// behind, orbiting around it.
//
// It is also an Interactable: walking up to its door and pressing the Use key
// runs the enter action (set with setEnterAction) straight away, with no
// panel. The stage then gives it the controller and the camera. While it is
// occupied it can't be used again.
//
// The camera has two views (setCameraView / toggleCameraView): from the
// driver's seat (the default), turning with the vehicle, or the old one from
// behind and above, orbiting it.
//
// The windshield breaks in a violent frontal crash (an ImpactDetector, fed by
// applyCollision and by the speed every frame): the damagedWindshield flag goes up
// and the cracked-glass model is shown instead of the intact one (setWindshieldModels).
//
// A frontal crash twice as hard as the least that breaks the windshield (ImpactDetector::severity
// >= 2) also wrecks the vehicle (wreck): the front of the body, the cracked glass and the lamps
// are replaced by crumpled copies, and the engine bay catches fire (flames and smoke: particle
// emitters, getFireEmitters, and an orange light, getFireLight). A fuse of 0.1 to 100 seconds (evenly distributed),
// picked at random, starts to run; when it ends the engine explodes (explodeEngine): fire, smoke
// and fuel splash like the mosquito's (effects/Explosion), a flash of light, a bang, a hop of the
// vehicle, and whoever is inside or near dies (setExplosionCallback). The engine is dead after
// that, and the fire goes on.
//
// The three rear-view mirrors (0 = the driver's side mirror, +x; 1 = the passenger's, -x; 2 = the
// central one, inside at the top of the windshield, which sees out through the rear window) show
// what is behind: each glass is a part of the RV that shows a texture (setMirrorTexture) which the
// main loop draws (they take turns, one a frame) from rearMirror()'s camera,
// at the glass, looking where the driver's line of sight bounces off it (a true reflection, a
// bit wider than a flat mirror's, as a convex one). The glass breaks (it is gone) when the front
// is wrecked.
//
// The handbrake lever, on the floor between the driver's seat and the middle of the cab (a rubber
// boot and a lever, setHandbrakeModels), follows the brake's switch (isHandbrakeOn, the Space key):
// pulled up and back (upright) while it is on, leaning forward when it is released, swinging between the two.
//
// From the moment the front is wrecked the RV is on fire (isOnFire, the flag onFire, which stays
// up): a fire alarm goes off. A red lamp on the right of the driver's panel blinks (a model of
// its own, setAlarmLampModel, with a dim red light on the dashboard each time it lights) and a
// beeping alarm sounds in the cab (heard from outside too, but fainter). The explosion cuts both.
//
// The explosion also throws out everything that is not the shell: the wheels, the dashboard,
// the steering wheel, the key, the needles and the broken windshield each fly off as a prop of
// their own, tumble, bounce on the floor and lie still (updateDebris).
//
// The cockpit: the dashboard, the ignition key and the two needles of the gauges
// (speed and fuel) are models of their own, added as parts of the RV
// (setCockpitModels): the speed needle follows the vehicle's speed, the fuel
// needle shows the fuel level (setFuel) and falls to empty when nobody is
// driving (ignition off), and the key turns in the lock when the driver gets in.
// With the headlights on the dashboard lights up: the marks of the gauges, the
// pilot lamps and the display glow (a model of its own, shown only then), the
// needles are drawn emissive, and a small dim orange light on each glowing
// component (getDashboardLights) gives off light on what is around it.
//
// It has two headlights: two spot lights at its front lamps (getHeadlights)
// that shine forward while they are on, and the lamps' lenses glow (a part
// that is only drawn then, see setHeadlightGlowModel).
//
// The headlights are not reliable: while they are on, a fault can start at any
// moment (lightFaultChance, percent per minute, which the player never sees).
// Then the lamps (and the dashboard, on the same circuit) flicker for a second
// or two, and either come back or, lightOutChance percent of the times, go out:
// the headlights are switched off and have to be switched on again.
class RV : public PlayableCharacter, public Interactable {
public:
  enum class CameraView {
    Cockpit, // the driver's eyes, inside the cab
    Chase,   // from behind and above, orbiting the vehicle
  };

protected:
  std::unique_ptr<VehicleBody> body; // created at the first update

private:
  Camera *camera = nullptr;
  float facing = 3.14159265f; // initial heading, radians (pi = towards -z)
  float throttle = 0.0f;      // -1 (reverse) .. 1 (forward)
  float steering = 0.0f;      // -1 (left) .. 1 (right)
  bool handbrakeOn = false;   // pulled by the driver (Space); it stays until released

  size_t wheelParts[4];              // front -x, front +x, rear -x, rear +x
  bool hasWheels = false;
  bool flatTires[4] = {false, false, false, false}; // same order (see punctureTire)
  bool occupied = false;             // someone is driving it
  bool keyOn = false;                // the key is turned: the engine is starting or running
  bool engineOn = false;             // the engine has caught and runs (see setEngine)
  bool headlightsOn = false;         // the light switch (what the player chose)
  bool parkedLights = false;         // got out with the lights on: they stay on without the engine
  // headlight faults (see the class comment)
  float lightFaultChance = 10.0f; // percent per minute with the lights on
  float lightOutChance = 35.0f;  // percent of the faults that end with them out
  float flickerTime = 0.0f;      // seconds left of the fault now, 0 = none
  float flickerChange = 0.0f;    // seconds until the lamps change again
  bool faultGoesOut = false;     // how the fault now will end
  float lampLevel = 1.0f;        // how bright the lamps are now, 0..1
  std::mt19937 random;
  std::shared_ptr<CompoundShape> hull; // its collision shape: a hollow box (see RV.cpp)
  // the door: opening swings it about the vertical through its front edge
  bool hasDoor = false;
  size_t doorPart = 0;
  bool doorLatched = true; // shut and clicked home: it stays so until somebody uses it
  float doorAngle = 0.0f;  // how far it is open now (radians, 0 = shut)
  float doorSpin = 0.0f;   // how fast it swings (rad/s, positive = opening)
  // The vehicle's own acceleration and spin (filtered), which the door feels (see updateDoor)
  glm::vec3 doorPrevVelocity = glm::vec3(0.0f), doorPrevSpin = glm::vec3(0.0f);
  glm::vec3 doorAccel = glm::vec3(0.0f), doorAlpha = glm::vec3(0.0f);
  void updateDoor(double dt);
  // The steering wheel as something to use: E next to it, inside, starts driving
  class Steering : public Interactable {
    RV &rv;

  public:
    explicit Steering(RV &rv) : rv(rv) {}
    std::string getInteractionName() const override { return "autocaravana"; }
    std::string getInteractionVerb() const override { return "conducir la"; }
    glm::vec3 getInteractionPoint() const override { return rv.driverStand(); }
    float getInteractionRange() const override { return 0.9f; } // (not from outside the walls)
    bool isInteractionAvailable() const override { return !rv.occupied; }
    bool usesDirectly() const override { return true; }
    void onUse(const glm::vec3 &) override {
      if (rv.enterAction && !rv.occupied)
        rv.enterAction();
    }
    void buildInterface(UIPanel &) override {}
  };
  Steering wheelUse{*this};
  // The passenger's seat as something to use: E next to it, inside, sits there
  class CopilotSeat : public Interactable {
    RV &rv;

  public:
    explicit CopilotSeat(RV &rv) : rv(rv) {}
    std::string getInteractionName() const override { return "asiento del copiloto"; }
    std::string getInteractionVerb() const override { return "sentarse en el"; }
    glm::vec3 getInteractionPoint() const override { return rv.copilotStand(); }
    float getInteractionRange() const override { return 0.9f; }
    bool isInteractionAvailable() const override { return !rv.copilotOccupied; }
    bool usesDirectly() const override { return true; }
    void onUse(const glm::vec3 &) override {
      if (rv.sitAction && !rv.copilotOccupied)
        rv.sitAction();
    }
    void buildInterface(UIPanel &) override {}
  };
  CopilotSeat copilotUse{*this};
  bool copilotOccupied = false;      // somebody sits in the passenger's seat
  std::function<void()> sitAction;   // what using that seat does
  size_t seatParts[2] = {0, 0};      // the pilot's seat and the copilot's
  bool hasSeats = false;
  RV(std::shared_ptr<Model> model, std::shared_ptr<CompoundShape> hull);

  void updateHeadlights(double dt);
  // The lamps shine: the switch is on and has power, and there is no fault that has them dark now
  bool lampsLit() const { return lightsActive() && lampLevel > 0.5f; }
  float uniform(float from, float to) {
    return std::uniform_real_distribution<float>(from, to)(random);
  }
  // the windshield
  ImpactDetector impact;
  bool damagedWindshield = false;
  bool hasWindshield = false;
  size_t windshieldPart = 0, brokenWindshieldPart = 0;

  void breakWindshield();
  void updateWindshieldParts();
  // A violent crash is found: breaks the windshield at once and, once the crash is over, wrecks
  // the vehicle if it was severe
  void handleImpact();

  // the wreck: crumpled front, fire, fuse and explosion
  size_t handbrakeBasePart = 0, handbrakeLeverPart = 0;
  bool hasHandbrake = false;
  float handbrakeAngle = 0.0f; // how far the lever is tipped forward from upright now (radians)
  void updateHandbrake(double dt);
  bool wrecked = false, exploded = false;
  bool onFire = false;     // the front is burning (from wreck() on): the alarm is on
  float alarmTime = 0.0f;  // seconds since the alarm went off
  size_t alarmPart = 0;
  bool hasAlarm = false;
  std::unique_ptr<Sound> alarmSound;
  bool alarmLit() const; // the lamp is lit now (it blinks)
  float fuse = 0.0f;       // seconds until the engine explodes (while wrecked)
  float flashTime = 0.0f;  // seconds left of the explosion's flash
  std::vector<std::shared_ptr<ParticleEmitter>> fire;  // flames and smoke of the burning front
  std::vector<std::shared_ptr<ParticleEmitter>> blast; // the explosion: fire, smoke, splash
  SoundEngine *soundEngine = nullptr;
  std::unique_ptr<Sound> bangSound;
  std::function<void(const glm::vec3 &, float)> explosionCallback;
  // A part thrown out by the explosion: where it is now (in the world), what it spins about
  // (its middle, in the model's frame), how far its middle is from its lowest point, and how
  // it moves
  struct Debris {
    size_t part;
    glm::mat4 world;
    glm::vec3 centre;
    float radius;
    glm::vec3 velocity;
    glm::vec3 axis;
    float spin; // rad/s
    bool resting;
  };
  std::vector<Debris> debris;
  void ejectParts(const glm::vec3 &from);
  void updateDebris(const Stage &stage, double dt);
  glm::vec3 engineBay() const; // where the fire is, in the world
  size_t mirrorPart[3] = {0, 0, 0};
  bool hasMirror[3] = {false, false, false}, mirrorShown[3] = {true, true, true};
  float mirrorAspect[3] = {0.5f, 0.5f, 3.0f};
  void updateMirrorParts() {
    for (int i = 0; i < 3; i++)
      if (hasMirror[i])
        setPartVisible(mirrorPart[i], mirrorShown[i] && !wrecked);
  }
  void updateFire(double dt);
  // the cockpit (see setCockpitModels)
  bool hasCockpit = false;
  bool hasSteeringWheel = false;
  size_t steeringWheelPart = 0;
  size_t keyPart = 0, speedNeedlePart = 0, fuelNeedlePart = 0, dashboardGlowPart = 0;
  float keyTurn = 0.0f;    // 0 = ignition off .. 1 = on
  float speedShown = 0.0f; // what the needles show now, 0..1 of their scales
  float fuelShown = 0.0f;
  float fuelPerMeter = 1.0f / 12000.0f; // see setFuelPerMeter
  float fuel = 0.75f;      // fuel level, 0..1: driving burns it (FUEL_PER_METER), empty = no engine

  // the engine: its speed (rpm) and its sound (see setEngineSound)
  EngineSimulator engineSim;
  std::unique_ptr<EngineSound> engineSound;

  void updateEngineSound(double dt, float speed);
  void setEngineRunning(bool on); // the engine caught / stopped: lights, gauges, driving follow
  void updateCockpit(double dt);
  void placeSteeringWheel();
  void updateDashboardLights(); // the dashboard glows with the headlights
  void updateLights();          // the lenses and the dashboard follow lightsActive()
  CameraView cameraView = CameraView::Cockpit;
  float chaseDistance = 12.0f, chaseHeight = 3.5f; // of the Chase view

  void applyCameraView();
  size_t glowPart = 0;               // lit lenses, drawn only with the lights on
  bool hasGlow = false;
  std::function<void()> enterAction; // what using the door does
  // Dust thrown up by each wheel (same order as wheelParts) while it drives on
  // sand
  std::vector<std::shared_ptr<ParticleEmitter>> dust;

  // Small, dark, opaque grains of sand flung by each wheel along with the dust
  std::vector<std::shared_ptr<ParticleEmitter>> grains;

  void updateDust(const Stage &stage);

  void placeWheels();
  void placeDoor();
  void ensureBody();
  // A client's copy (see writeNetState): what the server last said about it and how it shows it
  void updateReplica(double dt);
  const Stage *stage = nullptr;
  float netWheelLength[4] = {0.35f, 0.35f, 0.35f, 0.35f}, netWheelSteer[4] = {0, 0, 0, 0};
  bool netWheelGround[4] = {true, true, true, true};
  glm::vec3 netAngular = glm::vec3(0.0f);
  float netRpm = 0.0f, netLoad = 0.0f, netStarter = 0.0f, netFire = 0.0f;
  bool netCranking = false;
  float doorWanted = 0.0f; // the door's angle the server said
  float starterNow() const { return replica ? netStarter : engineSim.getStarter(); }

public:
  // The RV is a hollow box (see the class comment)
  explicit RV(std::shared_ptr<Model> model);

  // The door: its model (door.obj, in the frame of its hinge, which this puts in place); E at it
  // opens and closes it
  void setDoorModel(std::shared_ptr<Model> door);
  // The door is not latched shut (open, or swinging)
  bool isDoorOpen() const { return !doorLatched; }
  // Pushes it open (from latched) or shut (from anywhere else): it then moves by itself
  void toggleDoor();
  // The inside of its body, where one can walk (floor to roof, wall to wall, back to dashboard,
  // the cab too), in its frame: for the SafeSpace the map gives it
  static void interiorBox(glm::vec3 &centre, glm::vec3 &halfSize);
  // The steering wheel, as something to use from inside (give it to the stage's interactables)
  Interactable *steeringInteraction() { return &wheelUse; }
  // The two seats of the cab (seat.obj, in the seat's own frame: the pilot's on the +x side, the
  // copilot's on the -x one, both facing forward, standing on the floor)
  void setSeatModel(std::shared_ptr<Model> seat);
  // The passenger's seat, as something to use (give it to the stage's interactables); what using
  // it does is the map's (it sits the player there)
  Interactable *copilotInteraction() { return &copilotUse; }
  void setSitAction(std::function<void()> action) { sitAction = action; }
  bool isCopilotOccupied() const { return copilotOccupied; }
  void setCopilotOccupied(bool occupied) { copilotOccupied = occupied; }
  // Where the passenger stands on the floor behind his seat (world), and where his body rides when
  // he sits (feet, so that his eyes are where the pilot's are, on the other side)
  glm::vec3 copilotStand() const;
  glm::vec3 copilotSeatPosition() const;
  // Puts `camera` in the cockpit's eyes (the pilot's, or the copilot's), turning, pitching and
  // rolling with the vehicle (what followCamera does while somebody drives)
  void placeCockpitCamera(Camera &camera, bool copilot) const;
  // Where the driver stands on the floor inside, behind the steering wheel (world)
  glm::vec3 driverStand() const;

  float getMass() const override;
  // The stage the RV is in (a client's copy needs it for the dust and the pieces that fly off)
  void setStage(const Stage *s) { stage = s; }
  // What the server tells the clients about it (where it is goes with every object): its wheels,
  // its engine and lights, the door, the damage and the fuel
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  // The wheels' dust emitters: give them to the stage (Stage::addEmitter) to
  // have them updated and drawn
  const std::vector<std::shared_ptr<ParticleEmitter>> &getDust() const {
    return dust;
  }
  // How many rear-view mirrors it has: 0 = the driver's side mirror (+x), 1 = the passenger's (-x),
  // 2 = the central one
  static const int MIRRORS = 3;
  // The glass of mirror `side` shows this GL texture (RGBA, `aspect` = width / height of the picture)
  void setMirrorTexture(int side, unsigned int texture, float aspect);
  // Where the camera of mirror `side` goes (false if the glass is gone)
  bool rearMirror(int side, MirrorView &view) const;
  // Hides the glass while its own picture is drawn
  void showMirror(int side, bool show) {
    mirrorShown[side] = show;
    updateMirrorParts();
  }
  // The flames and smoke of the burning front, and the explosion's fire, smoke and splash: give
  // them to the stage too
  const std::vector<std::shared_ptr<ParticleEmitter>> &getFireEmitters() const { return fire; }
  const std::vector<std::shared_ptr<ParticleEmitter>> &getBlastEmitters() const { return blast; }
  // The fire's light (it flickers) and the explosion's flash, while there are any
  void getFireLight(std::vector<SpotLight> &lights) const;
  // The handbrake's boot and lever (handbrake_base.obj, handbrake_lever.obj: in the frame of the
  // lever's pivot, which this puts in place); the lever follows isHandbrakeOn
  void setHandbrakeModels(std::shared_ptr<Model> base, std::shared_ptr<Model> lever);
  // The red lamp of the fire alarm (dashboard_alarm.obj, in the frame of rv.obj): shown only
  // while it blinks on
  void setAlarmLampModel(std::shared_ptr<Model> lamp);
  // The front is burning: the fire alarm is on (set by wreck(); the explosion does not clear it)
  bool isOnFire() const { return onFire; }
  // Crumples the front and sets the engine on fire, with a fuse for the explosion (a no-op if it
  // is wrecked already). A severe frontal crash does it by itself.
  void wreck();
  bool isWrecked() const { return wrecked; }
  // Blows the engine up now (wrecking it first if need be)
  void explodeEngine();
  bool hasExploded() const { return exploded; }
  // What the explosion does to the people near it: called with its centre in the world and the
  // radius (m) of the blast, for the stage to kill whoever is inside
  void setExplosionCallback(std::function<void(const glm::vec3 &, float)> callback) {
    explosionCallback = callback;
  }
  // The wheels' sand grain emitters (same use as getDust)
  const std::vector<std::shared_ptr<ParticleEmitter>> &getGrains() const {
    return grains;
  }
  // The body of the vehicle is moved too
  void applyCollision(const glm::vec3 &push,
                      const glm::vec3 &velocityChange) override;

  // The physics of the RV model (see rv.obj), for the given gravity (m/s^2)
  // and top speed (m/s)
  static VehicleBody::Params vehicleParams(float gravity, float maxSpeed);

  // The wheels are separate parts, so they can follow the suspension: a
  // model for each side, centred on the wheel's axle (see generate_rv.py)
  void setWheelModels(std::shared_ptr<Model> negativeX,
                      std::shared_ptr<Model> positiveX);

  // The lit lenses of the headlights (see headlight_glow.obj): a part drawn
  // emissive, and only while the headlights are on
  void setHeadlightGlowModel(std::shared_ptr<Model> model);
  // The switch is on (the lights shine only if the engine is on, or if it was parked with them on: lightsActive)
  bool areHeadlightsOn() const { return headlightsOn; }
  bool lightsActive() const { return headlightsOn && (engineOn || parkedLights); }
  void setHeadlights(bool on);
  void toggleHeadlights() {
    if (engineOn) // (with the engine off the switch does nothing)
      setHeadlights(!headlightsOn);
  }
  // Headlight faults: the chance (percent per minute with them on) that one
  // starts, and the share (percent) of them that leave the headlights off
  void setLightFaultChance(float percentPerMinute) {
    lightFaultChance = glm::clamp(percentPerMinute, 0.0f, 100.0f);
  }
  float getLightFaultChance() const { return lightFaultChance; }
  void setLightOutChance(float percent) {
    lightOutChance = glm::clamp(percent, 0.0f, 100.0f);
  }
  float getLightOutChance() const { return lightOutChance; }
  // A fault right now (if the headlights are on): they flicker and then come
  // back or go out, by lightOutChance as always
  void startLightFault();
  bool isLightFaulty() const { return flickerTime > 0.0f; }
  // Adds the two spot lights (left and right, in the world) if they are on
  void getHeadlights(std::vector<SpotLight> &lights) const;
  // The dashboard's own light: with the headlights on, its glowing parts give off a dim
  // orange light that tints what is near them (the frame of the gauges, the switches...):
  // five small omni lights, one on each glowing component
  void getDashboardLights(std::vector<SpotLight> &lights) const;

  // The cockpit's models, in the frame of rv.obj: the dashboard, the ignition
  // key (key.obj), a gauge needle (needle.obj; it is used for two gauges) and
  // what glows with the headlights (dashboard_glow.obj, over the dashboard)
  void setCockpitModels(std::shared_ptr<Model> dashboard,
                        std::shared_ptr<Model> key,
                        std::shared_ptr<Model> needle,
                        std::shared_ptr<Model> dashboardGlow);
  // The steering wheel (steering_wheel.obj), in its own frame placed under the dashboard
  // (see setSteeringWheelModel in RV.cpp); it turns with the front wheels
  void setSteeringWheelModel(std::shared_ptr<Model> wheel);
  // Gives the engine a sound: it idles from the moment it starts and revs up with the
  // speed and the throttle (EngineSimulator, EngineSound). Without it, it is silent.
  void setEngineSound(SoundEngine &sound);
  float getEngineRpm() const { return engineSim.getRpm(); }
  // The two windshields, in the frame of rv.obj: the intact one, and the broken one (cracked
  // glass) that replaces it when the windshield is damaged
  void setWindshieldModels(std::shared_ptr<Model> intact, std::shared_ptr<Model> broken);
  bool isWindshieldDamaged() const { return damagedWindshield; }
  // The windshield as a surface, in the world: the middle of the glass, the way up along its
  // slope and its outward normal (for what gets stuck on it: see FollaCulos)
  void windshieldFrame(glm::vec3 &center, glm::vec3 &up, glm::vec3 &normal) const;
  // Is the world point `p` right in front of the vehicle, where its front would hit it? (within
  // its width, from near its bumper to a body length out)
  bool isInFront(const glm::vec3 &p) const;
  // Pushes a sphere of this radius out of the vehicle's body (its side profile, a convex
  // shape: for a ragdoll that falls on it)
  void pushOutOfBody(glm::vec3 &point, float radius) const;
  // Would the vehicle, moving, run over someone standing with his feet at this world point? (it
  // goes faster than a walking pace and the point is within its footprint, at body height)
  bool isRunningOver(const glm::vec3 &feet) const;
  // Its velocity in the world (m/s)
  glm::vec3 getVelocity() const { return body ? body->getVelocity() : glm::vec3(0.0f); }
  // How fast it goes forwards (m/s)
  float forwardSpeed() const { return body ? body->getForwardSpeed() : 0.0f; }
  void repairWindshield(); // the intact windshield again
  // Fuel level, 0 (empty) to 1 (full). Driving burns it in proportion to the speed; with none
  // left the engine does not push any more (shown on the gauge)
  void setFuel(float level) { fuel = glm::clamp(level, 0.0f, 1.0f); }
  float getFuel() const { return fuel; }
  // The cap of the fuel tank, in the world (on the -x side, behind the rear wheel), and the
  // outward normal of the wall it is on (something that sucks fuel out goes there)
  void fuelCap(glm::vec3 &position, glm::vec3 &normal) const;

  // The tyres: wheel i (0 front -x, 1 front +x, 2 rear -x, 3 rear +x; -x is the right-hand side)
  // can be burst. A flat tyre sinks that corner and drags, so the RV pulls towards that side
  // (VehicleBody::setFlat), and it is drawn squashed. They stay flat until repairTires.
  void punctureTire(int wheel);
  bool isTireFlat(int wheel) const { return wheel >= 0 && wheel < 4 && flatTires[wheel]; }
  void repairTires();
  // The middle of wheel i (its hub) in the world
  glm::vec3 wheelHub(int wheel) const;
  // How much of the tank a metre costs (default 1/12000: a full tank is 12 km)
  void setFuelPerMeter(float amount) { fuelPerMeter = amount; }

  CameraView getCameraView() const { return cameraView; }
  void setCameraView(CameraView view);
  void toggleCameraView() {
    setCameraView(cameraView == CameraView::Cockpit ? CameraView::Chase
                                                    : CameraView::Cockpit);
  }
  // The driver's eyes in the world (the Cockpit camera)
  glm::vec3 eyePosition() const;

  // What happens when the player uses the RV (the stage hands it the controls)
  void setEnterAction(std::function<void()> action) { enterAction = action; }
  // The engine starts off and only the Engine key switches it on or off: getting out leaves it
  // as it is (an empty RV with the engine running burns no
  // fuel), so getting back in finds it running.
  // If its lights are on when the player gets out they stay on (parkedLights: the switch still
  // gives them power, even if the engine is off) until somebody gets in again.
  void setOccupied(bool occupied) {
    parkedLights = !occupied && lightsActive();
    this->occupied = occupied;
  }
  // The engine (the ignition key): off, the vehicle can't be driven (it coasts and its brake
  // holds it), both gauge needles drop to empty whatever the real values, the key turns back
  // and the lights go out; the light switch keeps its state, so they come back on when the
  // engine starts again (the switch does nothing while it is off)
  bool isEngineOn() const { return engineOn; }
  void setEngine(bool on);
  // Turning the key on starts the engine only after the starter has cranked it (isEngineOn is
  // false meanwhile, and the lights, gauges and driving stay off); it may take several tries.
  bool isKeyOn() const { return keyOn; }
  // One press of the engine key: with the engine off it turns the key for ONE try (it fails 40 % of
  // the times: the key springs back and the next press tries again); running, it switches it off.
  // A press while a try is going on is ignored (one try per press).
  void toggleEngine() {
    if (keyOn && !engineOn)
      return;
    setEngine(!keyOn);
  }
  // Which way it faces (radians around +y, 0 = towards +z; the door is on its
  // +x side). Only before the first update: after it the physics rules.
  void setHeading(float radians) {
    facing = radians;
    setYaw(radians);
  }
  bool isOccupied() const { return occupied; }
  // World positions: the driver's seat (inside the body) and a point beside
  // the door (on the +x side), `outside` units away from the wall, on the floor
  glm::vec3 seatPosition() const;
  glm::vec3 doorPosition(float outside) const;
  // View directions (camera yaw: 0 looks towards -z, see Controller): along
  // the RV's heading, and away from the door
  float headingYaw() const;
  float doorYaw() const;

  // Interactable: open or close the door (to drive, see steeringInteraction)
  std::string getInteractionName() const override { return "puerta"; }
  std::string getInteractionVerb() const override { return isDoorOpen() ? "cerrar la" : "abrir la"; }
  glm::vec3 getInteractionPoint() const override;
  float getInteractionRange() const override { return 2.0f; }
  bool isInteractionAvailable() const override { return !occupied; }
  bool usesDirectly() const override { return true; }
  void onUse(const glm::vec3 &playerPosition) override;
  void buildInterface(UIPanel &panel) override {} // no panel

  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  // Stores the input; the camera heading is ignored on purpose
  void control(glm::vec2 dir, float up, float cameraYaw) override;
  // The handbrake the driver pulls or releases (Space): while it is pulled every wheel brakes
  // (VehicleBody::setHandbrake), until it is released. It keeps its state when the driver gets
  // out and in; getting out does not set it (an empty RV rolls if the ground slopes).
  bool isHandbrakeOn() const { return handbrakeOn; }
  void setHandbrakeOn(bool on) { handbrakeOn = on; }
  void toggleHandbrake() { handbrakeOn = !handbrakeOn; }
  void update(double dt) override;
  // The suspension and the floor: moves the RV
  bool contactFloor(const Stage &stage, double dt) override;
  // Its body is placed there too, upright, keeping its heading and at rest
  void teleport(const glm::vec3 &position) override;
  // Same: its body is placed upright, at rest, with the new heading
  void turn(float radians) override;
  // Adds the vehicle's own physics: speed, spin, wheels, occupied
  void describe(std::vector<std::string> &lines) const override;
  // The speed forwards, top speed, fuel, headlights and their faults, and the
  // windshield
  void getProperties(std::vector<Property> &properties) override;
};

#endif
