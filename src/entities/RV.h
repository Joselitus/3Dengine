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
#include "PlayableCharacter.h"
#include "SpotLight.h"
#include "VehicleBody.h"

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
  // the cockpit (see setCockpitModels)
  bool hasCockpit = false;
  bool hasSteeringWheel = false;
  size_t steeringWheelPart = 0;
  size_t keyPart = 0, speedNeedlePart = 0, fuelNeedlePart = 0, dashboardGlowPart = 0;
  float keyTurn = 0.0f;    // 0 = ignition off .. 1 = on
  float speedShown = 0.0f; // what the needles show now, 0..1 of their scales
  float fuelShown = 0.0f;
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

public:
  // The RV is a long box, not a pill
  explicit RV(std::shared_ptr<Model> model);

  float getMass() const override;
  // The wheels' dust emitters: give them to the stage (Stage::addEmitter) to
  // have them updated and drawn
  const std::vector<std::shared_ptr<ParticleEmitter>> &getDust() const {
    return dust;
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
  // as it is (an empty RV with the engine running stays put, its handbrake on, and burns no
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
  void toggleEngine() { setEngine(!keyOn); }
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

  // Interactable: get in at the door
  std::string getInteractionName() const override { return "autocaravana"; }
  std::string getInteractionVerb() const override { return "conducir la"; }
  glm::vec3 getInteractionPoint() const override { return doorPosition(0.4f); }
  float getInteractionRange() const override { return 2.5f; }
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
  // out (an empty RV brakes anyway) and in.
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
