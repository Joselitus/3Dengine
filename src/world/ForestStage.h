#ifndef FOREST_STAGE
#define FOREST_STAGE

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "VehicleStage.h"

// A very long road (3 km of asphalt, gently winding over rolling hills)
// surrounded on both sides by dense forest: spruces, pines, oaks and birches. The
// player starts on foot next to the RV, at the beginning of the road. The same
// day cycle as the desert (VehicleStage), with a line of trees on the horizon
// of the sky.
//
// Everything comes from assets/forest/ (see generate_forest.py and generate_trees.py): the
// terrain and the road, the road's centre line and where every tree stands. The forest is
// made of detailed trees (a trunk and branches with cards of leaves, in four species), each
// with three models: the normal one, a simpler one and an extremely low poly one (about 100-300
// triangles). The trees of the near forest (up to 45 m from the road) are objects that take the
// model that suits their distance to the player (low poly when far, the normal one as he nears;
// GameObject::addDetail) and are drawn only while near (Stage::setDrawDistance). The trees of the
// deep forest are always the extremely low poly one, and are merged into chunks of forest (one
// mesh per chunk, no object per tree). By the road the trunks are solid; 20 m into the forest an
// invisible wall follows the road on each side, and at both ends.
class ForestStage : public VehicleStage {
private:
  struct PathPoint {
    float x, z, height, tx, tz; // position, ground height and direction of travel
  };
  std::vector<PathPoint> path;

  bool loadPath(const std::string &file);
  // Every tree of the forest (hero_trees.txt): each its own object, with three levels of
  // detail that change with the distance; a solid trunk by the road, and the ones nearest the
  // road sway in the wind (GameObject::setSwayAmp). False if the data could not be read
  bool buildForest(const std::string &treesFile);
  // The invisible walls along the road (and across its two ends)
  void buildWalls();
  // A readable sign by the road, at path point `index`, on its left (side > 0) or right
  void addSign(size_t index, float side, const std::vector<std::string> &pages);
  // `box`: a solid, invisible box on the ground at (x, z), turned `yaw`
  void addWall(float x, float z, float yaw, const glm::vec3 &halfExtents);

public:
  ForestStage(FloorMode mode, SoundEngine &sound);
  // False if the map's assets are missing (a stage must not be used then)
  bool isValid() const { return valid; }

private:
  bool valid = false;
};

#endif
