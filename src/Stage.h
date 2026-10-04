#ifndef STAGE
#define STAGE

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "DynamicGameObject.h"
#include "GameObject.h"

// How the stage finds the height of its floor. Chosen when the stage is
// created and fixed for its whole life.
enum class FloorMode {
  // The floor mesh must be a regular grid of heights (x/z). The height is
  // interpolated from the grid: O(1), but no overhangs or tunnels.
  HeightField,
  // Any triangle mesh. A ray is cast straight down against the triangles,
  // which are indexed in a 2D grid so only a few are tested per query.
  DownwardRay,
};

// Everything that is loaded and placed in the world: the static scenery
// (GameObject) and the things that move (DynamicGameObject).
// Abstract: each concrete stage loads its own content and defines the rules
// applied to its dynamic objects.
class Stage {
private:
  const FloorMode floorMode;
  std::shared_ptr<Model> floor_mesh; // what the objects stand on
  // world-space bounds of the floor in x/z
  float minX = 0, maxX = 0, minZ = 0, maxZ = 0;

  // FloorMode::HeightField: heights[iz * nx + ix] at (x0 + ix*dx, z0 + iz*dz)
  int nx = 0, nz = 0;
  float x0 = 0, z0 = 0, dx = 1, dz = 1;
  std::vector<float> heights;

  // FloorMode::DownwardRay: world-space triangles and a grid of cells, each
  // listing the triangles whose x/z footprint overlaps it
  std::vector<glm::vec3> triVerts; // 3 per triangle
  int cellsX = 0, cellsZ = 0;
  float cellSize = 1;
  std::vector<std::vector<unsigned int>> cells;

  bool buildHeightField(const std::vector<glm::vec3> &verts);
  void buildTriangleGrid();
  bool heightFieldAt(float x, float z, float &height, glm::vec3 *normal) const;
  bool rayAt(float x, float z, float maxY, float &height,
             glm::vec3 *normal) const;

  std::map<std::string, std::shared_ptr<Model>> models; // loaded only once
  std::vector<std::shared_ptr<GameObject>> objects;
  std::vector<std::shared_ptr<DynamicGameObject>> dynamicObjects;

protected:
  explicit Stage(FloorMode mode) : floorMode(mode) {}

  // Applied to every dynamic object each update, right after the object has
  // moved dt seconds (collisions, bounds, AI, ...)
  virtual void apply(DynamicGameObject &object, double dt) = 0;

  // Keeps a dynamic object on the floor: it can't leave the floor's bounds or
  // sink into it, and it is `grounded` while it stands on it. Meant to be
  // called from apply().
  void collideWithFloor(DynamicGameObject &object) const;

public:
  virtual ~Stage() {}

  // Loads a model, or returns it if the stage already loaded that file
  std::shared_ptr<Model> loadModel(const std::string &path);

  std::shared_ptr<GameObject> add(std::shared_ptr<GameObject> object);
  std::shared_ptr<DynamicGameObject>
  addDynamic(std::shared_ptr<DynamicGameObject> object);

  const std::vector<std::shared_ptr<GameObject>> &getObjects() const {
    return objects;
  }
  const std::vector<std::shared_ptr<DynamicGameObject>> &
  getDynamicObjects() const {
    return dynamicObjects;
  }

  FloorMode getFloorMode() const { return floorMode; }

  // Sets the floor (once): the mesh placed at `position` in the world, with no
  // rotation or scale. The lookup structure for the stage's FloorMode is
  // built here. Returns false (and the stage has no floor) if it can't be:
  // with HeightField, when the mesh is not a regular grid.
  bool setFloor(std::shared_ptr<Model> mesh, const glm::vec3 &position);
  bool hasFloor() const { return floor_mesh != nullptr; }

  // Height of the floor at (x, z) and, optionally, its (upward) normal.
  // False if there is no floor there. In DownwardRay mode the ray starts at
  // maxY and finds the highest surface at or below it, so a floor above the
  // object (a ceiling, a bridge) is ignored; HeightField mode ignores maxY.
  bool floorAt(float x, float z, float &height, glm::vec3 *normal = nullptr,
               float maxY = 1e30f) const;

  // Advances every object by dt seconds and applies the stage rules to the
  // dynamic ones
  void update(double dt);
  // time feeds the shader's procedural animations (breathing)
  void Draw(Shader *shader, double time);
};

#endif
