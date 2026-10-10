#ifndef PROP_CATALOG
#define PROP_CATALOG

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "GameObject.h"

class Stage;

// Everything the map editor can put in a map: the props (static models: a cactus, a tree, a road
// shield...) and the entities (creatures and NPCs, made by the map: GameStage::spawnEntity). Each
// map brings its own scenery in code; the catalog lists what the game has so that any map can have
// more of it (see MapEdits).
struct PropType {
  std::string id;       // what MapEdits files call it
  std::string label;    // what the editor's menu shows
  std::string category; // the menu's group
  std::string model;    // under assets/
  std::string lod1, lod2; // simpler models for the distance (trees); empty: none
  float radius = 0.0f, height = 0.0f; // the solid part (a capsule); 0: not solid
  // An object of its own class (a building: its collision shape made of boxes, its door...) made from
  // `model`; null: a plain GameObject
  std::shared_ptr<GameObject> (*make)(Stage &stage, std::shared_ptr<Model> model) = nullptr;
  float sway = 0.0f;                  // wind (trees)
  float cullRadius = 0.0f;            // how far its parts reach, for not drawing it (0: the default)
  float scale = 1.0f;                 // size to start with
  float sink = 0.05f;                 // how far below the ground its origin goes, so it never floats
};

struct EntityType {
  std::string id;    // GameStage::spawnEntity's kind
  std::string label;
};

const std::vector<PropType> &propTypes();
const PropType *findProp(const std::string &id);
const std::vector<EntityType> &entityTypes();
const EntityType *findEntity(const std::string &id);

// A prop of `type` standing on the floor of `stage` at (x, z), `above` metres over it (negative:
// sunk), turned `yaw` and `scale` times its size; it is added to the stage. Null if the model
// cannot be loaded.
std::shared_ptr<GameObject> makeProp(Stage &stage, const PropType &type, float x, float z, float above, float yaw,
                                     float scale);

#endif
