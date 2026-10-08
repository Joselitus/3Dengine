#include "ForestStage.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

#include "MaterialMap.h"
#include "Readable.h"

using namespace glm;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {
const char *FOREST_DIR = "../assets/forest/";
const float DRAW_DISTANCE = 260.0f; // trees farther than this are not drawn (there is no fog to hide it)
const float LOD1_DISTANCE = 25.0f;  // from here a tree is drawn with fewer leaves...
const float LOD2_DISTANCE = 60.0f;  // ...and from here extremely low poly (see generate_trees.py)
const float CHUNK = 64.0f;          // metres: side of a chunk of the deep forest
const float CHUNK_RADIUS = 58.0f;   // ...and how far its trees reach from its centre
const float WALL_OFFSET = 20.5f;    // from the road's centre line
const float WALL_STEP = 10.0f;      // metres of road per wall box
const float START_Z_INDEX = 14;     // the RV waits this many metres into the road
}

bool ForestStage::loadPath(const string &file) {
  std::ifstream in(file);
  if (!in)
    return false;
  string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    PathPoint p;
    if (sscanf(line.c_str(), "%f %f %f %f %f", &p.x, &p.z, &p.height, &p.tx, &p.tz) == 5)
      path.push_back(p);
  }
  return path.size() > 100;
}

void ForestStage::addWall(float x, float z, float yaw, const vec3 &halfExtents) {
  // 3 m under the ground to 13 m over it, whatever the slope
  auto shape = make_shared<Box>(halfExtents, vec3(0.0f, 5.0f, 0.0f));
  static shared_ptr<Model> nothing = make_shared<Model>();
  auto wall = make_shared<GameObject>(nothing, shape);
  wall->setPosition(x, groundAt(x, z), z);
  wall->setYaw(yaw);
  wall->setVisible(false);
  add(wall);
}

void ForestStage::buildWalls() {
  for (size_t k = 0; k < path.size(); k += (size_t)WALL_STEP) {
    const PathPoint &p = path[k];
    float yaw = std::atan2(p.tx, p.tz);
    vec2 left(-p.tz, p.tx);
    for (float side : {1.0f, -1.0f})
      addWall(p.x + side * left.x * WALL_OFFSET, p.z + side * left.y * WALL_OFFSET, yaw,
              vec3(0.6f, 8.0f, WALL_STEP * 0.6f));
  }
  // across the two ends of the road
  for (int end = 0; end < 2; end++) {
    const PathPoint &p = end ? path.back() : path.front();
    float sign = end ? 1.0f : -1.0f;
    addWall(p.x + sign * p.tx * 6.0f, p.z + sign * p.tz * 6.0f, std::atan2(p.tx, p.tz),
            vec3(WALL_OFFSET + 1.0f, 8.0f, 0.6f));
  }
}

namespace {
// What goes into a merged mesh: the vertices of every instance of a material
struct Accumulator {
  vector<Vertex> vertices;
  vector<unsigned int> indices;
  vector<Texture> textures;
};
struct Chunk {
  vec3 centre;
  std::map<string, Accumulator> meshes; // by material name
};
}

bool ForestStage::buildForest(const string &file) {
  std::ifstream in(file);
  if (!in)
    return false;
  // Each model comes in three levels of detail; they are loaded once, whatever the trees
  struct Models {
    shared_ptr<Model> near, middle, far;
  };
  std::map<string, Models> models;
  auto modelsOf = [&](const string &name) -> Models & {
    auto found = models.find(name);
    if (found == models.end()) {
      string base = string(FOREST_DIR) + name;
      found = models.insert({name, {loadModel(base + ".obj"), loadModel(base + "_lod1.obj"),
                                    loadModel(base + "_lod2.obj")}}).first;
    }
    return found->second;
  };
  static shared_ptr<const CollisionShape> noShape = make_shared<Capsule>(0.1f, 0.5f);

  std::map<long long, Chunk> chunks; // the deep forest
  size_t count = 0, swaying = 0, solid = 0, deepCount = 0;
  string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    char name[64];
    float x, y, z, yaw, scale, trunk, height, crown;
    int animated, isSolid, deep;
    if (sscanf(line.c_str(), "%63s %f %f %f %f %f %d %d %d %f %f %f", name, &x, &y, &z, &yaw,
               &scale, &animated, &isSolid, &deep, &trunk, &height, &crown) != 12)
      continue;
    Models &m = modelsOf(name);
    count++;

    if (deep) {
      // The deep forest is always the extremely low poly model, so it needs no objects of its
      // own: the instances of a chunk of forest are merged into one mesh per material
      int cx = (int)std::floor(x / CHUNK), cz = (int)std::floor(z / CHUNK);
      Chunk &chunk = chunks[((long long)cx << 32) ^ (unsigned int)cz];
      if (chunk.meshes.empty())
        chunk.centre = vec3((cx + 0.5f) * CHUNK, y, (cz + 0.5f) * CHUNK);
      float c = std::cos(yaw), s = std::sin(yaw);
      vec3 at = vec3(x, y, z) - chunk.centre;
      for (const Mesh &mesh : m.far->meshes) {
        Accumulator &acc = chunk.meshes[mesh.getMaterialName()];
        if (acc.textures.empty())
          acc.textures = mesh.getTextures();
        unsigned int base = (unsigned int)acc.vertices.size();
        for (const Vertex &v : mesh.getVertices()) {
          Vertex w;
          vec3 p = v.Position * scale;
          w.Position = vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z) + at;
          w.Normal = vec3(c * v.Normal.x + s * v.Normal.z, v.Normal.y,
                          -s * v.Normal.x + c * v.Normal.z);
          w.TexCoords = v.TexCoords;
          acc.vertices.push_back(w);
        }
        for (unsigned int i : mesh.getIndices())
          acc.indices.push_back(base + i);
      }
      deepCount++;
      continue;
    }

    // The trunk is solid by the road (the shape is in the model's units: the scale applies to it
    // too); further in an invisible wall stops whoever walks, so the rest are not obstacles
    auto tree = make_shared<GameObject>(
        m.near, isSolid ? shared_ptr<const CollisionShape>(make_shared<Capsule>(trunk * 0.9f, height * 0.8f))
                        : noShape);
    // Far from the player it is the simple version, and it changes to the normal one as he nears
    tree->addDetail(m.middle, LOD1_DISTANCE);
    tree->addDetail(m.far, LOD2_DISTANCE);
    tree->setCollidable(isSolid != 0);
    tree->setPosition(x, y, z);
    tree->setYaw(yaw);
    tree->setScale(scale);
    // Only the trees by the road sway in the wind
    tree->setSwayAmp(animated ? 1.0f : 0.0f);
    tree->setCullRadius(crown * scale + 2.0f);
    add(tree);
    swaying += animated ? 1 : 0;
    solid += isSolid ? 1 : 0;
  }

  static shared_ptr<const CollisionShape> chunkShape = make_shared<Capsule>(0.5f, 1.0f);
  for (auto &entry : chunks) {
    Chunk &chunk = entry.second;
    auto model = make_shared<Model>();
    for (auto &m : chunk.meshes) {
      model->meshes.push_back(Mesh(m.second.vertices, m.second.indices, m.second.textures));
      model->meshes.back().setMaterialName(m.first);
    }
    auto object = make_shared<GameObject>(model, chunkShape);
    object->setPosition(chunk.centre.x, chunk.centre.y, chunk.centre.z);
    object->setCollidable(false); // the wall stops anybody before the deep forest
    object->setCullRadius(CHUNK_RADIUS);
    add(object);
  }
  fprintf(stderr,
          "ForestStage: %zu trees: %zu near (%zu swaying, %zu with a solid trunk) and %zu deep, "
          "merged in %zu chunks\n",
          count, count - deepCount, swaying, solid, deepCount, chunks.size());
  return count > 0;
}

void ForestStage::addSign(size_t index, float side, const vector<string> &pages) {
  const PathPoint &p = path[index];
  float x = p.x + side * (-p.tz) * 5.2f, z = p.z + side * p.tx * 5.2f;
  auto sign = make_shared<Readable>(loadModel("../assets/sign/sign.obj"), "Cartel", pages, 1.3f);
  sign->setPosition(x, groundAt(x, z), z);
  sign->setYaw(std::atan2(p.x - x, p.z - z)); // facing the road
  add(sign);
  interactables.push_back(sign.get());
}

ForestStage::ForestStage(FloorMode mode, SoundEngine &sound) : VehicleStage(mode) {
  // No music: the forest has only the wind (a day/night ambience would go here)
  loadAmbience("../assets/music/wind.wav");
  startDay();
  environment.forestHorizon = 1.0f; // trees on the horizon of the sky, not dunes
  setDrawDistance(DRAW_DISTANCE);

  string dir = FOREST_DIR;
  if (!loadPath(dir + "forest_path.txt")) {
    fprintf(stderr, "ForestStage: no road (%sforest_path.txt): run generate_forest.py\n", FOREST_DIR);
    return;
  }

  // The terrain: a regular grid of heights (the height field) with its materials
  // (asphalt on the road, grass everywhere else), drawn from the same mesh
  auto terrainModel = loadModel(dir + "forest_floor.obj");
  auto materials = MaterialMap::loadImage(dir + "forest_floor_materials.png");
  if (!materials) {
    fprintf(stderr, "ForestStage: no material map: the whole floor is grass\n");
    materials = MaterialMap::uniform(FloorMaterial::Grass);
  }
  auto ground = make_shared<GameObject>(terrainModel);
  ground->setCollidable(false); // it is the floor, not an obstacle
  ground->setCullRadius(1e9f);  // (always drawn)
  add(ground);
  if (!setFloor(terrainModel, vec3(0.0f), materials))
    return;
  auto road = make_shared<GameObject>(loadModel(dir + "road.obj"));
  road->setCollidable(false);
  road->setCullRadius(1e9f);
  add(road);

  // The RV a few metres into the road, facing along it; the player on foot by its
  // door, looking down the road
  const PathPoint &start = path[(size_t)START_Z_INDEX];
  float heading = std::atan2(start.tx, start.tz);
  createRV(sound, start.x, start.z, heading);
  vec3 door = rv->doorPosition(1.6f);
  createWalker(door.x, door.z - 0.5f, heading + 3.14159265f); // (looking along the road: 0 looks towards -z)

  addSign(4, 1.0f, {
      "CARRETERA FORESTAL. Tres kilometros de asfalto entre pinos y robles, sin una sola gasolinera.",
      "Conduce despacio: el firme esta viejo y de noche el bosque se cierra del todo.",
      "Si se acaba la gasolina, no hay nadie a quien pedir ayuda.",
  });
  addSign(path.size() - 12, -1.0f, {
      "FIN DE LA CARRETERA. Mas alla solo hay bosque.",
      "Da la vuelta: el camino de regreso es igual de largo.",
  });

  if (!buildForest(dir + "hero_trees.txt")) {
    fprintf(stderr, "ForestStage: no trees (%shero_trees.txt): run generate_forest.py\n", FOREST_DIR);
    return;
  }
  buildWalls();
  // Bob's ship lands on the road, a stretch ahead of the RV, its ramp towards it
  const PathPoint &landing = path[(size_t)START_Z_INDEX + 70];
  createAlienVisit(sound, vec3(landing.x, groundAt(landing.x, landing.z), landing.z),
                   std::atan2(start.x - landing.x, start.z - landing.z));
  valid = true;
}
