#ifndef MAP_EDITS
#define MAP_EDITS

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "EntityContext.h"
#include "GameObject.h"

class GameStage;

// What the map editor changed in a map, as a layer over it: the map is built as always (in code)
// and then the layer is applied to it (apply), the same on the server and on every client. It is
// kept in assets/edits/<map>.edit, a text file, one command per line:
//   grid NX NZ                        the terrain's grid the heights and materials refer to
//   height IX IZ DELTA                a grid point raised (or lowered) by DELTA metres
//   material CX CZ M                  a square of the terrain made of FloorMaterial M
//   prop TYPE X Z ABOVE YAW SCALE     a catalog prop (PropCatalog) added to the map
//   entity KIND X Z YAW               a creature or NPC added (GameStage::spawnEntity)
//   move s|d INDEX X ABOVE Z YAW      an object the map made (s static, d dynamic: its place in the
//                                     stage's list) put somewhere else, ABOVE metres over the floor
//   delete s|d INDEX                  ...taken away
// It needs no graphics: the server loads it too.
class MapEdits {
public:
  struct Prop {
    std::string type;
    float x, z, above, yaw, scale;
  };
  struct Entity {
    std::string kind;
    float x, z, yaw;
  };
  struct Move {
    bool dynamic;
    int index;
    glm::vec3 position; // y: over the floor
    float yaw;
    bool deleted;
  };

  // The grid of the terrain the edits are for (0: no terrain edits)
  int gridX = 0, gridZ = 0;
  std::map<unsigned int, float> heights;        // iz * nx + ix -> change
  std::map<unsigned int, unsigned char> materials; // cz * (nx - 1) + cx -> FloorMaterial
  std::vector<Prop> props;
  std::vector<Entity> entities;
  std::vector<Move> moves;

  // What apply() made, parallel to props and entities (the editor changes them later)
  std::vector<std::shared_ptr<GameObject>> propObjects, entityObjects;
  // How many static and dynamic objects the map had before the edits
  size_t baseStatics = 0, baseDynamics = 0;

  // The file of a map's edits
  static std::string pathFor(const std::string &mapName);
  bool load(const std::string &path);
  bool save(const std::string &path) const;
  bool empty() const {
    return heights.empty() && materials.empty() && props.empty() && entities.empty() && moves.empty();
  }

  // Puts the layer over the map just built: terrain first (what stood on the ground goes up or down
  // with it), then the objects moved or taken away, then the new ones
  void apply(GameStage &stage, EntityContext &context);

  // Edits of an object of the map (adds the move, or changes the one it has)
  Move &moveOf(bool dynamic, int index);
};

// The edits file of `mapName`, applied to `stage` if there is one
void applyMapEdits(GameStage &stage, const std::string &mapName, EntityContext &context);

#endif
