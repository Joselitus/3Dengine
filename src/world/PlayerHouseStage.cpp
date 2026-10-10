#include "PlayerHouseStage.h"

#include <cmath>
#include <cstdint>

#include "MaterialMap.h"
#include "PoissonDisk.h"
#include "SoundEngine.h"

using namespace glm;

namespace {
const float GROUND_Y = 0.0f;
const float HALF_SIZE = 100.0f; // the floor is 200 x 200 m (generate_player_house.py, SIZE)
const float MARGIN = 6.0f;      // no props this close to the edge
const float MORNING = 10.0f;

// A cheap repeatable "random" 0..1 from a place
float hash2(int x, int z) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return (float)((h ^ (h >> 16)) & 0xffffff) / 16777215.0f;
}
} // namespace

PlayerHouseStage::PlayerHouseStage(FloorMode mode, SoundEngine &sound) : VehicleStage(mode) {
  loadMusic("../assets/music/desert.wav");
  loadAmbience("../assets/music/wind.wav");
  groundFallback = GROUND_Y;

  // The floor: sand (the RV is slower on it)
  if (!setFloor(loadModel("../assets/player_house/house_floor.obj"), vec3(0.0f, GROUND_Y, 0.0f),
                MaterialMap::uniform(FloorMaterial::Sand)))
    fprintf(stderr, "PlayerHouseStage: could not set the floor\n");
  {
    auto ground = std::make_shared<GameObject>(loadModel("../assets/player_house/house_floor.obj"));
    ground->setPosition(0.0f, GROUND_Y, 0.0f);
    ground->setCollidable(false); // it is the floor, not an obstacle (before add)
    add(ground);
  }

  // The well: a solid cylinder round its curb and its posts
  auto well = std::make_shared<GameObject>(loadModel("../assets/player_house/well.obj"),
                                           std::make_shared<Capsule>(1.25f, 2.6f));
  well->setPosition(WELL_X, groundAt(WELL_X, WELL_Z), WELL_Z);
  well->setYaw(0.5f); // (its roller runs along x, turned a little)
  add(well);

  scatterProps();

  // The RV facing +z, its door (+x) towards the start, and the player on foot beside it
  createRV(sound, 0.0f, 0.0f, 0.0f);
  createWalker(3.0f, 4.0f);

  startDay();
  setTimeOfDay(MORNING);
}

// Cacti (two models) and a few rocks all over, away from the start, the well and the edge
void PlayerHouseStage::scatterProps() {
  std::shared_ptr<Model> models[4] = {loadModel("../assets/desert/cactus_a.obj"),
                                      loadModel("../assets/desert/cactus_b.obj"),
                                      loadModel("../assets/desert/rock_a.obj"),
                                      loadModel("../assets/desert/rock_b.obj")};
  vec2 well(WELL_X, WELL_Z);
  std::vector<vec2> spots = poissonDisk(vec2(-HALF_SIZE + MARGIN), vec2(HALF_SIZE - MARGIN), PROP_SPACING,
                                        20261010u, 30, [&](const vec2 &p) {
                                          return length(p) > CLEARING + 4.0f && length(p - well) > 6.0f;
                                        });
  for (const vec2 &p : spots) {
    float r1 = hash2((int)(p.x * 10.0f), (int)(p.y * 10.0f));
    float r2 = hash2((int)(p.y * 7.0f) + 13, (int)(p.x * 7.0f) - 5);
    int model = r1 < 0.78f ? (int)(r2 * 2.0f) % 2 : 2 + (int)(r2 * 2.0f) % 2; // mostly cacti
    auto prop = std::make_shared<GameObject>(models[model]);
    prop->setPosition(p.x, groundAt(p.x, p.y) - 0.05f, p.y);
    prop->setYaw(r1 * 6.2831853f * 7.0f);
    prop->setScale(model < 2 ? 0.9f + 0.9f * r2 : 1.0f + 1.2f * r2);
    add(prop);
  }
}
