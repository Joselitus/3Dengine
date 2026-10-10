#ifndef PLAYER_HOUSE_STAGE
#define PLAYER_HOUSE_STAGE

#include "VehicleStage.h"

// The "player_house" map: a predominantly flat desert (assets/player_house: 200 x 200 m of sand with
// barely a ripple), a few cacti and rocks here and there (scattered with a minimum spacing, the
// seed is fixed) and a traditional stone well with a wooden frame, roller, rope and bucket not far
// from the start, where the player is on foot beside the RV. The day goes by as in the desert
// (VehicleStage).
class PlayerHouseStage : public VehicleStage {
private:
  void scatterProps();

public:
  // The well's place, and the distance (m) it keeps from the start
  static constexpr float WELL_X = -14.0f, WELL_Z = 11.0f;
  // Cacti and rocks at least this far (m) from each other, and from the well and the start
  static constexpr float PROP_SPACING = 17.0f;
  static constexpr float CLEARING = 9.0f;

  PlayerHouseStage(FloorMode mode, SoundEngine &sound);
};

#endif
