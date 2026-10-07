#ifndef PINE_FOREST_STAGE
#define PINE_FOREST_STAGE

#include <memory>

#include "GameStage.h"

class RV;
class SoundEngine;
class Walker;

// The forest map ("Bosque"): a 200 x 200 m floor of needles and moss with gentle bumps
// (assets/forest), tall low-poly pines scattered with a minimum spacing (poissonDisk, the way a
// forest tool such as SpeedTree populates one) round a clearing where the player starts on foot
// beside the RV, and a day that goes by as in the desert (the sun crosses the sky).
//
// The shadows of the trees: an invisible canopy (Environment::canopyMask) built from the trees
// themselves: for three heights, the cross-section of every crown there, with gaps between the
// needles; the shader shades the sunlight of each point by where its ray to the sun crosses those
// planes, so the ground and the trees get the dappled shadows of the crowns, long when the sun is
// low, and they move with it.
//
// The player and the RV work as in the desert's TestStage (get in at the door with E, out with
// Shift; the flashlight, the headlights, the engine...).
class PineForestStage : public GameStage {
private:
  std::shared_ptr<RV> rv;
  std::shared_ptr<Walker> walker;
  bool inVehicle = false;
  unsigned int canopyTexture = 0;

  void enterRV();
  float groundAt(float x, float z) const;
  void plantTrees();
  // The canopy's mask from the trees (see the class comment): `trees` are (x, base y, z, height)
  void buildCanopy(const std::vector<glm::vec4> &trees);

protected:
  void onTimeChanged() override;
  void apply(DynamicGameObject &object, double dt) override;

public:
  // Pines at least this far (m) from each other, none within CLEARING (m) of the middle
  static constexpr float TREE_SPACING = 9.5f;
  static constexpr float CLEARING = 16.0f;
  // The crowns (assets/forest/generate_forest.py: CROWN_BASE and CROWN_RADIUS, of the height)
  static constexpr float CROWN_BASE = 0.22f, CROWN_RADIUS = 0.2f;
  // The canopy: the heights of its three planes over the ground (m), its texture's size (texels)
  static constexpr float CANOPY_LOW = 6.0f, CANOPY_MID = 11.0f, CANOPY_HIGH = 16.0f;
  static const int CANOPY_TEXELS = 1024;

  PineForestStage(FloorMode mode, SoundEngine &sound);
  ~PineForestStage() override;

  bool interactionsEnabled() const override { return !inVehicle && !isPlayerDead(); }
  void toggleHeadlights() override;
  void toggleEngine() override;
  void toggleHandbrake() override;
  void toggleVehicleCamera() override;
  void leaveVehicle() override;
  void getSpotLights(std::vector<SpotLight> &lights) const override;
};

#endif
