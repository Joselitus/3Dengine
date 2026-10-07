#ifndef VEHICLE_STAGE
#define VEHICLE_STAGE

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "FollaCulos.h"
#include "GameStage.h"
#include "RV.h"
#include "SoundEngine.h"
#include "SpeechSynthesizer.h"
#include "Walker.h"

// What the maps where the player drives the RV have in common (the day desert,
// the forest road): the penguin on foot (a Walker, first person) and the RV
// he can get into (E at its door) and out of (Left Shift), the controls that
// go with it (headlights, engine, handbrake, camera) and the daylight cycle
// (a sun that crosses the sky: blue by day, orange at dusk and dawn, dark with
// stars at night). Each map builds its own scenery, the floor and whatever
// stands on it, in its constructor, using createRV / createWalker (and
// createCreature, if it wants the night creature), and sets its clock with
// startDay.
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
  std::shared_ptr<Walker> walker;       // the penguin on foot
  std::shared_ptr<FollaCulos> creature; // the last night creature made (null if the map has none)
  std::vector<std::shared_ptr<FollaCulos>> creatures; // all of them (each lights its eyes)
  bool inVehicle = false;               // the penguin is inside the RV
  float groundFallback = 0.0f;          // ground height where there is no floor

  explicit VehicleStage(FloorMode mode) : GameStage(mode) {}

  // The RV on the floor at (x, z), facing `heading` (0 = +z), with its models,
  // its dust and its door; it is added to the stage and registered as usable
  void createRV(SoundEngine &sound, float x, float z, float heading);
  // The penguin on foot at (x, z): the player (first person)
  void createWalker(float x, float z);
  // A night creature at (x, z) (a map may have several: see `creatures`), wired to the player, the RV and the clock. The
  // map may still add prey (setPreyQuery)
  void createCreature(SoundEngine &sound, SpeechSynthesizer &speech, float x, float z);
  // The procedural sky and the clock: DAY_DURATION seconds a day, starting at START_HOUR
  void startDay();

  // The penguin gets into the RV: it is hidden inside its body and goes
  // wherever the RV goes, and the RV gets the controls and the camera
  void enterRV();

  // Ground height at (x, z), or groundFallback where there is no floor
  float groundAt(float x, float z) const {
    return GameStage::groundAt(x, z, groundFallback);
  }

  // The daylight cycle: the sun crosses the sky (east at 6:00, highest at
  // noon, west at 18:00). The sky, the fog and the light follow it. The music
  // fades out as the sun sets (until only the ambience remains).
  void onTimeChanged() override;

  // Everything dynamic stays on the floor (the penguin inside the RV just
  // rides in it)
  void apply(DynamicGameObject &object, double dt) override;

public:
  // Interactions are for the penguin on foot, not while driving
  bool interactionsEnabled() const override { return !inVehicle && !isPlayerDead(); }

  // The player (the penguin or the RV) is always drawn, wherever it is
  bool edgeCullExempt(const GameObject &object) const override {
    return &object == rv.get() || &object == walker.get();
  }

  // F: the headlights while the penguin is driving, its flashlight on foot
  void toggleHeadlights() override;
  // R: the engine, only while the penguin is driving
  void toggleEngine() override;
  // Space: the handbrake, only while the penguin is driving
  void toggleHandbrake() override;
  // C: inside the RV or behind it, only while the penguin is driving
  void toggleVehicleCamera() override;
  void getSpotLights(std::vector<SpotLight> &lights) const override;
  // The penguin gets out at the RV's door, on foot and in first person
  void leaveVehicle() override;
};

#endif
