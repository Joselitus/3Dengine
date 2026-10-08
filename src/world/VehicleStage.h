#ifndef VEHICLE_STAGE
#define VEHICLE_STAGE

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "AlienVisit.h"
#include "Flatwoods.h"
#include "FollaCulos.h"
#include "GameStage.h"
#include "RV.h"
#include "SoundEngine.h"
#include "SpeechSynthesizer.h"
#include "Walker.h"

// What the maps where the players drive the RV have in common (the day desert,
// the forest road): the penguins on foot (a Walker each, first person) and the RV
// they can get into (E at its door) and out of (Left Shift), one at a time, the controls that
// go with it (headlights, engine, handbrake, camera) and the daylight cycle
// (a sun that crosses the sky: blue by day, orange at dusk and dawn, dark with
// stars at night). Each map builds its own scenery, the floor and whatever
// stands on it, in its constructor, using createRV / createWalker (and
// createCreature, if it wants the night creature), and sets its clock with
// startDay.
//
// This is the server's side of the game (or a client's copy of it, which only shows what the
// server says): what each player is doing (in the RV, in Bob's ship, paralysed...) is in his
// Player, and what the creatures and Bob do is done for whoever of them is nearest.
class VehicleStage : public GameStage {
protected:
  // PenguinoAnimado.fbx holds two takes of the same dance; take 0 (".002") has
  // the right flipper detached from the body and the feet in the air, take 1
  // (".003") is the clean one.
  static constexpr unsigned int PENGUIN_ANIMATION = 1;
  static constexpr float CAR_CAMERA_DISTANCE = 12.0f;
  static constexpr float CAR_CAMERA_HEIGHT = 3.5f;
  static constexpr float EYE_HEIGHT = 1.6f;     // first person, above the feet
  static constexpr float DAY_DURATION = 360.0f; // real seconds per 24 h
  static constexpr float START_HOUR = 12.0f;    // the game starts at midday
  // What is left of the light with no sun (it comes from straight above): very
  // little, so that at night it is hard to see anything without the headlights
  const glm::vec3 NIGHT_LIGHT = glm::vec3(0.022f, 0.025f, 0.04f);

  std::shared_ptr<RV> rv;
  std::shared_ptr<FollaCulos> creature; // the last night creature made (null if the map has none)
  std::vector<std::shared_ptr<FollaCulos>> creatures; // all of them (each lights its eyes)
  AlienVisit alien;                     // Bob and his ship, if the map has them (createAlienVisit)
  // The creatures can't reach a player who drives the RV or flies the ship, or who is on foot in a
  // SafeSpace (inside the RV): they behave as when he drives (this is their "in a vehicle" query)
  bool playerSheltered(const Player &p) const {
    return p.inVehicle || p.inSaucer || isSheltered(p.walker->getPosition());
  }
  // The Flatwoods monster (createRV makes it, in every map with the RV, at night) and the inside of
  // the RV, where it comes for the player
  std::shared_ptr<Flatwoods> flatwoods;
  std::shared_ptr<SafeSpace> rvInside;
  int flatwoodsVictim = -1; // the player it goes for (the nearest one in the RV)
  bool playerInRV(const Player &p) const {
    return p.inVehicle || (rvInside && rvInside->contains(p.walker->getPosition()));
  }
  static constexpr float POSSESSED_SPEED = 1.3f; // m/s, a possessed body's pace
  float groundFallback = 0.0f;          // ground height where there is no floor

  explicit VehicleStage(FloorMode mode) : GameStage(mode) {}

  // The RV on the floor at (x, z), facing `heading` (0 = +z), with its models,
  // its dust and its door; it is added to the stage and registered as usable
  void createRV(SoundEngine &sound, float x, float z, float heading);
  // Where the players' penguins appear: at (x, z), facing `yaw` (they are made when a player
  // joins: see GameStage::addPlayer)
  void createWalker(float x, float z, float yaw = 0.0f);
  // A night creature at (x, z) (a map may have several: see `creatures`), wired to the players, the RV and the clock. The
  // map may still add prey (setPreyQuery)
  void createCreature(SoundEngine &sound, SpeechSynthesizer &speech, float x, float z);
  // Bob comes at night in his ship, which lands on `landing` with its ramp towards `rampYaw`
  // (see AlienVisit): he takes the player if he catches him on foot
  void createAlienVisit(SoundEngine &sound, const glm::vec3 &landing, float rampYaw);
  // The Flatwoods monster, wired to the RV and the players (createRV calls it)
  void createFlatwoods();
  void startPossession(Player &p);
  void endPossession(Player &p);
  void updatePossession(Player &p, double dt);
  // Out of the RV's seat onto the cab's floor (the leave key, or the monster's doing)
  void getOutOfRV(Player &p);
  // The procedural sky and the clock: DAY_DURATION seconds a day, starting at START_HOUR
  void startDay();

  // The acting player's penguin gets into the RV (if nobody drives it): it is hidden inside its
  // body and goes wherever the RV goes, and the RV gets his controls and camera
  void enterRV();
  // The acting player's penguin gets into Bob's ship (its ramp is down): it rides in it, and the
  // ship gets his controls and camera (from behind and above)
  void enterSaucer();
  // The nearest player alive to `from` (null if there is none): the one the creatures go for
  Player *nearestAlive(const glm::vec3 &from);

  // Ground height at (x, z), or groundFallback where there is no floor
  float groundAt(float x, float z) const {
    return GameStage::groundAt(x, z, groundFallback);
  }

  // The daylight cycle: the sun crosses the sky (east at 6:00, highest at
  // noon, west at 18:00). The sky, the fog and the light follow it. The music
  // fades out as the sun sets (until only the ambience remains).
  void onTimeChanged() override;

  // Players get out of what they drive when they die or leave
  void onPlayerGone(Player &p) override;
  bool canInteract(const Player &p) const override {
    return !p.dead && !p.inVehicle && !p.inSaucer;
  }
  // Paralysed by Bob's ray or held by Bob: no moving or looking
  bool isImmobilized(const Player &p) const override;
  void respawn(Player &p) override;
  // The ship nobody flies comes down and stands (see tick)
  void onTick(double dt) override;

  // Everything dynamic stays on the floor (a penguin inside the RV just rides in it)
  void apply(DynamicGameObject &object, double dt) override;

public:
  float playerParalysis() const override;
  float struggleProgress() const override;
  float alienPresence() const override;
  void endAlienHiss() override;
  bool playerAiming() const override { return local && local->inSaucer && alien.saucer->isAiming(); }
  // (the Flatwoods monster only shows in the mirrors)
  void setMirrorView(bool inMirror) override {
    if (flatwoods)
      flatwoods->setMirrorView(inMirror);
  }

  // F: the headlights while the penguin is driving, its flashlight on foot, the ray gun of the ship
  void toggleHeadlights(Player &p) override;
  // R: the engine, only while the penguin is driving
  void toggleEngine(Player &p) override;
  // Q: the legs of Bob's ship, while the penguin flies it
  void toggleShipLegs(Player &p) override;
  void fire(Player &p, const glm::vec3 &eye, const glm::vec3 &direction) override;
  // Space: the handbrake, only while the penguin is driving
  void toggleHandbrake(Player &p) override;
  // C: inside the RV or behind it, only while the penguin is driving
  void toggleVehicleCamera(Player &p) override;
  // The penguin gets out at the RV's door, on foot and in first person; or out of Bob's ship, if it
  // stands on its legs (else, the key brings the ship down: see Saucer). On foot, held by Bob, the
  // key is his struggle to get free.
  void leaveVehicle(Player &p) override;
  void refuel(Player &p) override;

  bool rearMirror(int side, MirrorView &view) const override;
  void setRearMirrorTexture(int side, unsigned int texture, float aspect) override;
  void showRearMirror(int side, bool show) override;
  void getSpotLights(std::vector<SpotLight> &lights) const override;
};

#endif
