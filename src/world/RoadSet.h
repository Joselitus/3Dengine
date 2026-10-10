#ifndef ROAD_SET
#define ROAD_SET

#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "FloorMaterial.h"

// A road the map editor drew: a spline (centripetal Catmull-Rom) through control points on the
// ground (x, z), with a width and a surface. Kept in assets/edits/<map>.edit (see MapEdits).
struct Road {
  enum Type { Asphalt = 0, Dirt = 1 };
  int type = Asphalt;
  float width = 8.0f;
  bool closed = false; // the last point joins the first
  std::vector<glm::vec2> points;
};

// The centre line of a road at even steps: where, which way it goes, and how far along it is
struct RoadSample {
  glm::vec2 position;
  glm::vec2 tangent; // unit
  float distance;    // along the road
};

// The spline of a road cut into samples `step` metres apart (a closed road ends on its first
// point again, with distance = its length). Fewer than two points: nothing.
std::vector<RoadSample> sampleRoad(const Road &road, float step);

// The roads of a stage, for the game logic: what is the ground made of where a road runs
// (Stage::materialAt asks). No graphics: the server has them too.
class RoadSet {
private:
  static constexpr float CELL = 8.0f;
  struct Segment {
    glm::vec2 a, b;
    float halfWidth;
    int type;
  };
  std::vector<Segment> segments; // later roads are later (they are on top)
  std::unordered_map<long long, std::vector<unsigned int>> grid;

public:
  // Replaces the roads (rebuilds the lookup)
  void set(const std::vector<Road> &roads);
  bool empty() const { return segments.empty(); }
  // The surface of the road at (x, z), if one covers it
  bool surfaceAt(float x, float z, FloorMaterial &material) const;
};

#endif
