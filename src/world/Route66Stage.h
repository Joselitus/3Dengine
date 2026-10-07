#ifndef ROUTE66_STAGE
#define ROUTE66_STAGE

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "VehicleStage.h"

// "Ruta 66": an extremely long road (20 km of two-lane asphalt through the high desert, with
// telephone poles, Route 66 shields, billboards, saguaros, rocks and red mesas on the horizon)
// with a gas station right in the middle of it, 10 km in, at the road's right-hand side. The
// RV's tank (12 km, the RV's default: enough to reach the station but not the end of the road), so the station is the only place to refuel: its four pumps (FuelPump, E on foot) fill
// the tank of an RV parked next to them. The same day cycle as the other driving maps
// (VehicleStage).
//
// The terrain, the road, the station and the other models come from assets/route66/
// (generate_route66.py): the terrain is a height field 400 m wide that follows the road, level
// around the station and at its ends, and a huge flat plane carries the desert on to the
// horizon (that is why the camera sees 4 km here, see GameStage::farPlane). Invisible walls
// run along both sides of the terrain and across the ends of the road.
//
// The measures of the station (STATION_Z, the pumps, the building...) are repeated from
// generate_route66.py (stage_constants.txt).
class Route66Stage : public VehicleStage {
private:
  struct PathPoint {
    float x, z, height, tx, tz; // position, ground height and direction of travel
  };
  std::vector<PathPoint> path;

  bool loadPath(const std::string &file);
  // The point of the road's centre line at z (interpolated)
  PathPoint pointAt(float z) const;
  void buildStation();
  void buildRoadside();
  void buildWalls();
  // A solid, invisible box standing on the ground at (x, z), turned `yaw`
  void addBox(float x, float z, float yaw, const glm::vec3 &halfExtents);
  // A readable sign beside the road at z, on its right (side < 0: -x) or left
  void addSign(float z, float side, const std::vector<std::string> &pages);

public:
  Route66Stage(FloorMode mode, SoundEngine &sound, SpeechSynthesizer &speech);
  // False if the map's assets are missing (a stage must not be used then)
  bool isValid() const { return valid; }

private:
  bool valid = false;
};

#endif
