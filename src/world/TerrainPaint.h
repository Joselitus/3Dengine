#ifndef TERRAIN_PAINT
#define TERRAIN_PAINT

#include <map>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "FloorMaterial.h"
#include "GameObject.h"
#include "Model.h"

class Stage;

// The picture of the terrain squares whose material the map editor changed: a layer of quads a few
// centimetres over the floor, each with the texture of its material (sand, asphalt, grass), that
// follows the floor when it is raised or lowered. The floor's own picture stays as it was: this
// only covers it where it was painted. (The material itself, for the game's logic, is the Stage's:
// Stage::setTerrainMaterial.) It is one mesh per material for each block of CHUNK x CHUNK squares,
// made when something in the block is first painted, with every square there already: the ones not
// painted are collapsed to a point.
class TerrainPaint {
public:
  static constexpr int CHUNK = 32;

private:
  struct Chunk {
    int mesh[(int)FloorMaterial::Count];
    Chunk() {
      for (int &m : mesh)
        m = -1;
    }
  };
  Stage &stage;
  std::shared_ptr<Model> model = std::make_shared<Model>();
  std::shared_ptr<GameObject> object;
  glm::vec3 origin; // the object's place: the middle of the floor (the quads are relative to it)
  int cellsX, cellsZ;
  bool mainDiagonal; // the floor's triangles cut each square from (ix, iz) to (ix + 1, iz + 1)
  std::vector<unsigned char> shown; // per square: the material painted there, or 255
  std::map<long long, Chunk> chunks;
  std::vector<std::pair<size_t, size_t>> touched; // per mesh: the vertex range to send again

  static constexpr unsigned char NONE = 255;
  static constexpr float LIFT = 0.04f; // metres over the floor

  void writeCell(const Chunk &chunk, int cx, int cz, FloorMaterial material, bool show);
  int meshFor(Chunk &chunk, int chunkX, int chunkZ, FloorMaterial material);
  void touch(int mesh, size_t first, size_t last);

public:
  explicit TerrainPaint(Stage &stage);
  // Shows square (cx, cz) painted with `material`, or (show = false) not painted
  void set(int cx, int cz, FloorMaterial material, bool show);
  // The heights of the grid points in [ix0, ix1] x [iz0, iz1] changed (Stage::commitTerrain)
  void heightsChanged(int ix0, int iz0, int ix1, int iz1);
  // Sends what changed to the GPU
  void flush();
};

#endif
