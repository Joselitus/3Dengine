#ifndef PINE_FOREST_STAGE
#define PINE_FOREST_STAGE

#include <memory>

#include "VehicleStage.h"

// The pine forest map ("Bosque de pinos"): a 200 x 200 m floor of needles and moss with gentle
// bumps (assets/pine_forest), tall low-poly pines scattered with a minimum spacing (poissonDisk, the way a
// forest tool such as SpeedTree populates one) round a clearing where the player starts on foot
// beside the RV, and a day that goes by as in the desert (the sun crosses the sky).
//
// The shadows: an invisible canopy (Environment::canopyMask) over the whole map, as if there were
// much taller trees above the pines: three planes (CANOPY_LOW/MID/HIGH, above the pines' tops)
// covered with the crowns of those trees, with gaps between the crowns and between their leaves;
// the shader shades the sunlight of each point by where its ray to the sun crosses those planes,
// so everything (the ground, the pines, the RV) is in dappled shade with patches of sun, which
// move as the sun does.
//
// At night Bob's ship lands in the clearing and Bob comes out (AlienVisit).
//
// The player, the RV and the day are those of every VehicleStage (as in the desert).
class PineForestStage : public VehicleStage {
private:
  unsigned int canopyTexture = 0;

  void plantTrees();
  // The canopy's mask (see the class comment)
  void buildCanopy();

public:
  // Pines at least this far (m) from each other, none within CLEARING (m) of the middle
  static constexpr float TREE_SPACING = 9.5f;
  static constexpr float CLEARING = 16.0f;
  // The canopy: the heights of its three planes over the ground (m: above the pines, 18-27 m
  // tall), its texture's size (texels)
  static constexpr float CANOPY_LOW = 28.0f, CANOPY_MID = 33.0f, CANOPY_HIGH = 38.0f;
  static const int CANOPY_TEXELS = 1024;

  PineForestStage(FloorMode mode, SoundEngine &sound);
  ~PineForestStage() override;
};

#endif
