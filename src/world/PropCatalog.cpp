#include "PropCatalog.h"

#include "CollisionShape.h"
#include "Stage.h"

using namespace std;
using namespace glm;

namespace {
PropType prop(const char *id, const char *label, const char *category, const char *model, float radius = 0.0f,
              float height = 0.0f, float scale = 1.0f) {
  PropType t;
  t.id = id;
  t.label = label;
  t.category = category;
  t.model = model;
  t.radius = radius;
  t.height = height;
  t.scale = scale;
  return t;
}

// A tree of the Bosque map: three levels of detail, a solid trunk, and the wind
PropType tree(const char *name, const char *label, float trunk, float height, float crown) {
  PropType t = prop(name, label, "Arboles", "", trunk * 0.9f, height * 0.8f);
  string base = string("forest/hero_") + name + ".obj";
  t.id = string("tree_") + name;
  t.model = base;
  t.lod1 = string("forest/hero_") + name + "_lod1.obj";
  t.lod2 = string("forest/hero_") + name + "_lod2.obj";
  t.sway = 1.0f;
  t.cullRadius = crown + 2.0f;
  return t;
}

vector<PropType> makePropTypes() {
  vector<PropType> v;
  // The desert's scenery is not solid-shaped by hand: the default shape fits the model
  v.push_back(prop("cactus_a", "Cactus A", "Desierto", "desert/cactus_a.obj"));
  v.push_back(prop("cactus_b", "Cactus B", "Desierto", "desert/cactus_b.obj"));
  v.push_back(prop("rock_a", "Roca A", "Desierto", "desert/rock_a.obj"));
  v.push_back(prop("rock_b", "Roca B", "Desierto", "desert/rock_b.obj"));
  PropType puddle = prop("puddle", "Charco", "Desierto", "water/puddle.obj");
  puddle.sink = -0.03f; // (it floats a little over the sand)
  v.push_back(puddle);
  v.push_back(tree("oak_a", "Roble A", 0.55f, 13.74f, 8.57f));
  v.push_back(tree("oak_b", "Roble B", 0.48f, 10.11f, 6.89f));
  v.push_back(tree("oak_c", "Roble C", 0.62f, 16.21f, 10.57f));
  v.push_back(tree("pine_a", "Pino A", 0.34f, 17.0f, 4.12f));
  v.push_back(tree("pine_b", "Pino B", 0.30f, 14.0f, 4.13f));
  v.push_back(tree("spruce_a", "Abeto A", 0.38f, 16.0f, 3.90f));
  v.push_back(tree("spruce_b", "Abeto B", 0.32f, 13.0f, 3.84f));
  v.push_back(tree("spruce_c", "Abeto C", 0.42f, 19.0f, 3.91f));
  v.push_back(tree("birch_a", "Abedul A", 0.20f, 13.23f, 4.86f));
  v.push_back(tree("birch_b", "Abedul B", 0.22f, 14.31f, 7.20f));
  for (int i = 0; i < 3; i++) {
    string n = to_string(i);
    PropType t = prop(("pine_forest_" + n).c_str(), ("Pino bajo " + n).c_str(), "Bosque de pinos",
                      ("pine_forest/pine_" + n + ".obj").c_str(), 0.7f, 12.0f);
    t.sink = 0.1f;
    t.cullRadius = 8.0f;
    v.push_back(t);
  }
  PropType pole = prop("pole", "Poste telefonico", "Ruta 66", "route66/pole.obj", 0.2f, 9.0f);
  pole.cullRadius = 6.0f;
  v.push_back(pole);
  PropType shield = prop("shield", "Escudo Ruta 66", "Ruta 66", "route66/shield.obj", 0.1f, 2.0f);
  shield.cullRadius = 3.0f;
  v.push_back(shield);
  PropType board = prop("billboard", "Valla publicitaria", "Ruta 66", "route66/billboard.obj", 0.3f, 5.0f);
  board.cullRadius = 8.0f;
  v.push_back(board);
  PropType mesa = prop("mesa", "Meseta roja", "Ruta 66", "route66/mesa.obj");
  mesa.cullRadius = 2800.0f;
  mesa.sink = 0.0f;
  v.push_back(mesa);
  return v;
}
} // namespace

const vector<PropType> &propTypes() {
  static const vector<PropType> types = makePropTypes();
  return types;
}

const PropType *findProp(const string &id) {
  for (const PropType &t : propTypes())
    if (t.id == id)
      return &t;
  return nullptr;
}

const vector<EntityType> &entityTypes() {
  static const vector<EntityType> types = {
      {"folla_culos", "FollaCulos (criatura nocturna)"},
      {"mosquito", "Mosquito gigante"},
      {"pingu", "Pingu (NPC)"},
      {"gnome", "Gnomo de jardin (cuchillo)"},
  };
  return types;
}

const EntityType *findEntity(const string &id) {
  for (const EntityType &t : entityTypes())
    if (t.id == id)
      return &t;
  return nullptr;
}

shared_ptr<GameObject> makeProp(Stage &stage, const PropType &type, float x, float z, float above, float yaw,
                                float scale) {
  shared_ptr<Model> model = stage.loadModel("../assets/" + type.model);
  if (!model)
    return nullptr;
  shared_ptr<GameObject> object;
  if (type.height > 0.0f)
    object = make_shared<GameObject>(model, make_shared<Capsule>(type.radius, type.height));
  else
    object = make_shared<GameObject>(model);
  if (!type.lod1.empty())
    object->addDetail(stage.loadModel("../assets/" + type.lod1), 25.0f);
  if (!type.lod2.empty())
    object->addDetail(stage.loadModel("../assets/" + type.lod2), 60.0f);
  // (a prop with no solid part but a model: scenery you walk through, like a mesa or a puddle,
  // keeps the model's own shape; those the catalog gives a size are solid)
  if (type.id == "mesa" || type.id == "puddle")
    object->setCollidable(false);
  if (type.sway > 0.0f)
    object->setSwayAmp(type.sway);
  if (type.cullRadius > 0.0f)
    object->setCullRadius(type.cullRadius * scale);
  float ground = 0.0f;
  stage.floorAt(x, z, ground);
  object->setPosition(x, ground + above - type.sink, z);
  object->setYaw(yaw);
  object->setScale(scale);
  stage.add(object);
  return object;
}
