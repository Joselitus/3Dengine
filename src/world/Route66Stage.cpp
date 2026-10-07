#include "Route66Stage.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>

#include "FuelPump.h"
#include "MaterialMap.h"
#include "Readable.h"

using namespace glm;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {
const char *DIR = "../assets/route66/";
const float ROAD_END_Z = 20000.0f;   // generate_route66.py
const float STATION_Z = 10000.0f;    // ...the middle of the road
const float PUMP_X = -17.0f;         // the pumps, on the right of the road (-x), under the canopy
const float PUMP_ZS[4] = {-7.5f, -2.5f, 2.5f, 7.5f};
const float BUILDING_X = -30.0f, BUILDING_W = 22.0f, BUILDING_D = 11.0f, BUILDING_H = 4.2f;
const float CANOPY_X = -16.0f, CANOPY_W = 30.0f, CANOPY_D = 11.0f;
const float PYLON_X = -6.2f, PYLON_Z = -26.0f;
const float WALL_X = 190.0f;         // the walls along the terrain (it reaches +-200 m)
const float WALL_STEP = 200.0f;      // metres of wall per box
const float START_Z = 40.0f;         // the RV waits this far into the road
const float DRAW_DISTANCE = 600.0f;  // small things are drawn up to here; the mesas have a longer reach
const float FAR_PLANE = 4000.0f;
const float CREATURE_ZS[2] = {3000.0f, 7000.0f}; // where the night creatures stand
const float POLE_STEP = 60.0f;       // metres between telephone poles
const float SHIELD_STEP = 1000.0f;   // ...and between Route 66 shields
const float BILLBOARD_STEP = 2500.0f;
}

bool Route66Stage::loadPath(const string &file) {
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

Route66Stage::PathPoint Route66Stage::pointAt(float z) const {
  // one point every 10 m, from z = 0
  float f = clamp(z / 10.0f, 0.0f, (float)(path.size() - 1) - 1e-3f);
  size_t i = (size_t)f;
  float t = f - (float)i;
  const PathPoint &a = path[i], &b = path[i + 1];
  PathPoint p;
  p.x = a.x + (b.x - a.x) * t;
  p.z = a.z + (b.z - a.z) * t;
  p.height = a.height + (b.height - a.height) * t;
  p.tx = a.tx + (b.tx - a.tx) * t;
  p.tz = a.tz + (b.tz - a.tz) * t;
  return p;
}

void Route66Stage::addBox(float x, float z, float yaw, const vec3 &halfExtents) {
  // from a little under the ground up to twice its half height, whatever the slope
  auto shape = make_shared<Box>(halfExtents, vec3(0.0f, halfExtents.y - 1.0f, 0.0f));
  static shared_ptr<Model> nothing = make_shared<Model>();
  auto wall = make_shared<GameObject>(nothing, shape);
  wall->setPosition(x, groundAt(x, z), z);
  wall->setYaw(yaw);
  wall->setVisible(false);
  add(wall);
}

void Route66Stage::buildWalls() {
  for (float z = 0.0f; z < ROAD_END_Z; z += WALL_STEP)
    for (float side : {1.0f, -1.0f})
      addBox(side * WALL_X, z + WALL_STEP / 2, 0.0f, vec3(0.6f, 8.0f, WALL_STEP / 2 + 1.0f));
  // across the two ends of the road
  addBox(0.0f, -6.0f, 0.0f, vec3(WALL_X, 8.0f, 0.6f));
  addBox(0.0f, ROAD_END_Z + 6.0f, 0.0f, vec3(WALL_X, 8.0f, 0.6f));
}

void Route66Stage::addSign(float z, float side, const vector<string> &pages) {
  PathPoint p = pointAt(z);
  float x = p.x + side * 5.6f;
  auto sign = make_shared<Readable>(loadModel("../assets/sign/sign.obj"), "Cartel", pages, 1.3f);
  sign->setPosition(x, groundAt(x, z), z);
  sign->setYaw(std::atan2(p.x - x, 0.0f)); // facing the road
  add(sign);
  interactables.push_back(sign.get());
}

void Route66Stage::buildStation() {
  // The building, canopy, pylon and the forecourt are one model each, in the road's frame
  // (the road is straight and on x = 0 around the station)
  float y = groundAt(0.0f, STATION_Z);
  for (const char *file : {"station.obj", "apron.obj"}) {
    auto object = make_shared<GameObject>(loadModel(string(DIR) + file));
    object->setCollidable(false); // the solid parts are the boxes below
    object->setPosition(0.0f, y, STATION_Z);
    object->setCullRadius(90.0f);
    add(object);
  }
  addBox(BUILDING_X, STATION_Z, 0.0f, vec3(BUILDING_D / 2, BUILDING_H / 2 + 1.0f, BUILDING_W / 2));
  for (float sx : {-1.0f, 1.0f})
    for (float sz : {-1.0f, 1.0f})
      addBox(CANOPY_X + sx * (CANOPY_D / 2 - 0.8f), STATION_Z + sz * (CANOPY_W / 2 - 1.2f), 0.0f,
             vec3(0.25f, 3.0f, 0.25f));
  addBox(PYLON_X, STATION_Z + PYLON_Z, 0.0f, vec3(0.2f, 5.0f, 0.2f));

  // The pumps: each fills the RV's tank if it is parked by the pumps
  FuelPump::Tank tank;
  tank.level = [this]() { return rv->getFuel(); };
  tank.fill = [this]() { rv->setFuel(1.0f); };
  tank.distance = [this](const vec3 &from) {
    vec3 d = rv->getPosition() - from;
    return std::sqrt(d.x * d.x + d.z * d.z);
  };
  auto pumpModel = loadModel(string(DIR) + "pump.obj");
  for (float pz : PUMP_ZS) {
    auto pump = make_shared<FuelPump>(pumpModel, tank);
    pump->setPosition(PUMP_X, y + 0.18f, STATION_Z + pz); // on its island
    add(pump);
    interactables.push_back(pump.get());
  }
}

void Route66Stage::buildRoadside() {
  std::mt19937 rng(66);
  auto uniform = [&rng](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };

  // Telephone poles along the left of the road (+x), as on the old highway
  auto pole = loadModel(string(DIR) + "pole.obj");
  for (float z = POLE_STEP; z < ROAD_END_Z - 20.0f; z += POLE_STEP) {
    PathPoint p = pointAt(z);
    float x = p.x + 8.5f;
    auto object = make_shared<GameObject>(pole, make_shared<Capsule>(0.2f, 9.0f));
    object->setPosition(x, groundAt(x, z), z);
    object->setYaw(std::atan2(p.tx, p.tz)); // the crossarm across the road
    object->setCullRadius(6.0f);
    add(object);
  }

  // The Route 66 shield every kilometre (right of the road) and the billboards
  auto shield = loadModel(string(DIR) + "shield.obj");
  for (float z = SHIELD_STEP; z < ROAD_END_Z; z += SHIELD_STEP) {
    PathPoint p = pointAt(z);
    float x = p.x - 5.2f;
    auto object = make_shared<GameObject>(shield, make_shared<Capsule>(0.1f, 2.0f));
    object->setPosition(x, groundAt(x, z), z);
    object->setCullRadius(3.0f);
    add(object);
  }
  auto billboard = loadModel(string(DIR) + "billboard.obj");
  int side = 1;
  for (float z = BILLBOARD_STEP * 0.5f; z < ROAD_END_Z; z += BILLBOARD_STEP, side = -side) {
    float zz = std::fabs(z - STATION_Z) < 120.0f ? z + 200.0f : z;
    PathPoint p = pointAt(zz);
    float x = p.x + side * 16.0f;
    auto object = make_shared<GameObject>(billboard, make_shared<Capsule>(0.3f, 5.0f));
    object->setPosition(x, groundAt(x, zz), zz);
    object->setCullRadius(8.0f);
    add(object);
  }

  // Cacti and rocks scattered either side, thicker near the road
  auto cactusA = loadModel("../assets/desert/cactus_a.obj");
  auto cactusB = loadModel("../assets/desert/cactus_b.obj");
  auto rockA = loadModel("../assets/desert/rock_a.obj");
  auto rockB = loadModel("../assets/desert/rock_b.obj");
  size_t count = 0;
  for (float z = 60.0f; z < ROAD_END_Z - 40.0f; z += 28.0f) {
    for (int k = 0; k < 3; k++) {
      float zz = z + uniform(0.0f, 28.0f);
      float lateral = 9.0f + 150.0f * std::pow(uniform(0.0f, 1.0f), 2.0f);
      float sideSign = uniform(0.0f, 1.0f) < 0.5f ? 1.0f : -1.0f;
      // keep the forecourt, the pylon and the station clear
      if (std::fabs(zz - STATION_Z) < 70.0f && sideSign < 0.0f && lateral < 70.0f)
        continue;
      PathPoint p = pointAt(zz);
      float x = p.x + sideSign * lateral;
      shared_ptr<Model> model;
      float scale;
      if (k == 2) {
        model = uniform(0.0f, 1.0f) < 0.5f ? rockA : rockB;
        scale = uniform(0.8f, 2.6f);
      } else {
        model = uniform(0.0f, 1.0f) < 0.5f ? cactusA : cactusB;
        scale = uniform(0.9f, 1.8f);
      }
      auto object = make_shared<GameObject>(model);
      object->setPosition(x, groundAt(x, zz), zz);
      object->setYaw(uniform(0.0f, 6.2831853f));
      object->setScale(scale);
      object->setCullRadius(4.0f * scale);
      add(object);
      count++;
    }
  }

  // Red mesas on the horizon, far from the road: not solid, drawn from kilometres away
  auto mesa = loadModel(string(DIR) + "mesa.obj");
  for (int k = 0; k < 44; k++) {
    float z = uniform(0.0f, ROAD_END_Z);
    float x = pointAt(z).x + (uniform(0.0f, 1.0f) < 0.5f ? 1.0f : -1.0f) * uniform(450.0f, 1500.0f);
    float scale = uniform(0.7f, 2.2f);
    auto object = make_shared<GameObject>(mesa);
    object->setCollidable(false);
    object->setPosition(x, 0.0f, z);
    object->setYaw(uniform(0.0f, 6.2831853f));
    object->setScale(scale);
    object->setCullRadius(2800.0f);
    add(object);
  }
  fprintf(stderr, "Route66Stage: %zu cacti and rocks, poles every %.0f m, 44 mesas\n", count, POLE_STEP);
}

Route66Stage::Route66Stage(FloorMode mode, SoundEngine &sound, SpeechSynthesizer &speech) : VehicleStage(mode) {
  loadMusic("../assets/music/desert.wav");
  loadAmbience("../assets/music/wind.wav");
  startDay();
  setDrawDistance(DRAW_DISTANCE);
  farPlane = FAR_PLANE;

  string dir = DIR;
  if (!loadPath(dir + "route66_path.txt")) {
    fprintf(stderr, "Route66Stage: no road (%sroute66_path.txt): run generate_route66.py\n", DIR);
    return;
  }

  // The terrain (a height field) with its materials: asphalt on the road and the forecourt, sand
  auto terrainModel = loadModel(dir + "r66_floor.obj");
  auto materials = MaterialMap::loadImage(dir + "r66_floor_materials.png");
  if (!materials) {
    fprintf(stderr, "Route66Stage: no material map: the whole floor is sand\n");
    materials = MaterialMap::uniform(FloorMaterial::Sand);
  }
  auto ground = make_shared<GameObject>(terrainModel);
  ground->setCollidable(false);
  ground->setCullRadius(1e9f);
  add(ground);
  if (!setFloor(terrainModel, vec3(0.0f), materials))
    return;
  for (const char *file : {"far_ground.obj", "road.obj"}) {
    auto object = make_shared<GameObject>(loadModel(dir + file));
    object->setCollidable(false);
    object->setCullRadius(1e9f);
    add(object);
  }

  // The RV at the start of the road, facing along it; the player on foot by its door
  const PathPoint start = pointAt(START_Z);
  float heading = std::atan2(start.tx, start.tz);
  createRV(sound, start.x, start.z, heading);
  rv->setFuel(1.0f);
  vec3 door = rv->doorPosition(1.6f);
  createWalker(door.x, door.z - 0.5f);
  cameraDistance = 0.0f; // first person
  cameraHeight = EYE_HEIGHT;
  cameraYaw = heading + 3.14159265f; // (0 looks towards -z: this is along the road)

  addSign(10.0f, -1.0f, {
      "RUTA 66. La Carretera Madre: veinte kilometros de asfalto viejo y ni un alma.",
      "A mitad de camino, en el kilometro 10, hay una gasolinera. Es la unica.",
      "Con el deposito lleno llegas a doce kilometros: no te pases de largo.",
  });
  addSign(STATION_Z - 150.0f, -1.0f, {
      "GASOLINERA A 150 METROS. Gasolina, hielo y cafe recien hecho.",
      "Abierto las 24 horas. Aparca junto a los surtidores y pulsa E en uno de ellos.",
  });
  addSign(ROAD_END_Z - 12.0f, -1.0f, {
      "FIN DE LA RUTA 66. Santa Monica queda mucho mas lejos, al otro lado del desierto.",
      "Da la vuelta: el camino de regreso es igual de largo.",
  });

  // A couple of night creatures waiting by the road, one in each half (at night they come for
  // the player from wherever they are; by day they keep away)
  for (float z : CREATURE_ZS) {
    float x = pointAt(z).x + 30.0f;
    createCreature(sound, speech, x, z);
  }

  buildStation();
  buildRoadside();
  buildWalls();
  valid = true;
}
