#include "PineForestStage.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <GL/glew.h>

#include "AnimatedModel.h"
#include "MaterialMap.h"
#include "PoissonDisk.h"
#include "RV.h"
#include "SoundEngine.h"
#include "Walker.h"

using namespace glm;

namespace {
const float GROUND_Y = 0.0f;           // where there is no floor
const float HALF_SIZE = 100.0f;        // the floor is 200 x 200 m (generate_forest.py, SIZE)
const float TREE_MARGIN = 6.0f;        // no trees this close to the edge
const float EYE_HEIGHT = 1.6f;
const float CAR_CAMERA_DISTANCE = 12.0f, CAR_CAMERA_HEIGHT = 3.5f;
const float DAY_DURATION = 360.0f;     // real seconds per 24 h, as in the desert
const float START_HOUR = 10.0f;        // a morning sun: long, slanting shadows
const unsigned int PENGUIN_ANIMATION = 1; // (see TestStage)
const vec3 NIGHT_LIGHT(0.022f, 0.025f, 0.04f);
// How much light a patch of needles stops, in one plane of the canopy (three planes overlap
// under a crown: the middle of its shadow is darker than its edge)
const float LEAF_DENSITY = 0.7f;

float smooth01(float a, float b, float x) {
  float t = clamp((x - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// Value noise (0..1) over the world's x, z with cells `cell` metres wide: the gaps between needles
float hash2(int x, int z) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return (float)((h ^ (h >> 16)) & 0xffffff) / 16777215.0f;
}
float valueNoise(float x, float z, float cell) {
  float fx = x / cell, fz = z / cell;
  int x0 = (int)std::floor(fx), z0 = (int)std::floor(fz);
  float tx = fx - x0, tz = fz - z0;
  tx = tx * tx * (3.0f - 2.0f * tx);
  tz = tz * tz * (3.0f - 2.0f * tz);
  float a = mix(hash2(x0, z0), hash2(x0 + 1, z0), tx);
  float b = mix(hash2(x0, z0 + 1), hash2(x0 + 1, z0 + 1), tx);
  return mix(a, b, tz);
}
} // namespace

PineForestStage::PineForestStage(FloorMode mode, SoundEngine &sound) : GameStage(mode) {
  // Only the wind (the desert's music would not fit)
  loadAmbience("../assets/music/wind.wav");
  setSky(loadModel("../assets/sky/skydome_plain.obj"), 3);
  environment.skyDunes = false; // (no dunes on the horizon here)
  setDayDuration(DAY_DURATION);

  // The floor: needles and moss on gentle bumps, firm to drive on (like a dirt track: no sand)
  if (!setFloor(loadModel("../assets/pine_forest/forest_floor.obj"), vec3(0.0f, GROUND_Y, 0.0f),
                MaterialMap::uniform(FloorMaterial::Asphalt)))
    fprintf(stderr, "PineForestStage: could not set the floor\n");
  {
    auto ground = std::make_shared<GameObject>(loadModel("../assets/pine_forest/forest_floor.obj"));
    ground->setPosition(0.0f, GROUND_Y, 0.0f);
    ground->setCollidable(false); // (before add)
    add(ground);
  }

  plantTrees();

  // The RV in the clearing, facing +z (its door, on +x, towards the start)
  rv = std::make_shared<RV>(loadModel("../assets/rv/rv.obj"));
  rv->setPosition(0.0f, groundAt(0.0f, 0.0f), 0.0f);
  rv->setHeading(0.0f);
  rv->setWheelModels(loadModel("../assets/rv/wheel_negx.obj"),
                     loadModel("../assets/rv/wheel_posx.obj"));
  rv->setWindshieldModels(loadModel("../assets/rv/windshield.obj"),
                          loadModel("../assets/rv/windshield_broken.obj"));
  rv->setCockpitModels(loadModel("../assets/rv/dashboard.obj"), loadModel("../assets/rv/key.obj"),
                       loadModel("../assets/rv/needle.obj"),
                       loadModel("../assets/rv/dashboard_glow.obj"));
  rv->setSteeringWheelModel(loadModel("../assets/rv/steering_wheel.obj"));
  rv->setEngineSound(sound);
  rv->setHeadlightGlowModel(loadModel("../assets/rv/headlight_glow.obj"));
  rv->setMaxSpeed(20.0f);
  rv->setGravity(25.0f);
  addDynamic(rv);
  for (const auto &emitter : rv->getDust())
    addEmitter(emitter);
  for (const auto &emitter : rv->getGrains())
    addEmitter(emitter);
  rv->setEnterAction([this]() { enterRV(); });
  interactables.push_back(rv.get());

  // The player: the penguin on foot, beside the RV, in first person
  walker = std::make_shared<Walker>(std::make_shared<AnimatedModel>(
      "../assets/ping/PenguinoAnimado.fbx", false, PENGUIN_ANIMATION));
  walker->setPosition(3.0f, groundAt(3.0f, 4.0f), 4.0f);
  walker->setGravity(25.0f);
  addDynamic(walker);
  player = walker;
  cameraDistance = 0.0f;
  cameraHeight = EYE_HEIGHT;

  setTimeOfDay(START_HOUR); // (the light, once everything is there)
}

PineForestStage::~PineForestStage() {
  if (canopyTexture)
    glDeleteTextures(1, &canopyTexture);
}

float PineForestStage::groundAt(float x, float z) const { return GameStage::groundAt(x, z, GROUND_Y); }

// Pines all over, at least TREE_SPACING apart (Poisson disk), none in the clearing nor at the
// edge; each one of the three models, turned and sized at random
void PineForestStage::plantTrees() {
  std::shared_ptr<Model> models[3] = {loadModel("../assets/pine_forest/pine_0.obj"),
                                      loadModel("../assets/pine_forest/pine_1.obj"),
                                      loadModel("../assets/pine_forest/pine_2.obj")};
  // how tall each model is (the pill the engine fits to it)
  float heights[3];
  for (int i = 0; i < 3; i++) {
    GameObject probe(models[i]);
    vec3 lo, hi;
    probe.getShape().bounds(probe.getPose(), lo, hi);
    heights[i] = hi.y - lo.y;
  }
  vec2 lo(-HALF_SIZE + TREE_MARGIN), hi(HALF_SIZE - TREE_MARGIN);
  std::vector<vec2> spots = poissonDisk(lo, hi, TREE_SPACING, 20261007u, 30,
                                        [](const vec2 &p) { return length(p) > CLEARING; });
  std::vector<vec4> trees;
  for (size_t i = 0; i < spots.size(); i++) {
    // (a cheap repeatable "random" from the place)
    float r1 = hash2((int)(spots[i].x * 10.0f), (int)(spots[i].y * 10.0f));
    float r2 = hash2((int)(spots[i].y * 7.0f) + 13, (int)(spots[i].x * 7.0f) - 5);
    int model = (int)(r1 * 3.0f) % 3;
    float scale = 0.8f + 0.45f * r2;
    // the trunk only: the crowns are high above anything that walks or drives
    auto tree = std::make_shared<GameObject>(models[model],
                                             std::make_shared<Capsule>(0.7f, 12.0f));
    float y = groundAt(spots[i].x, spots[i].y);
    tree->setPosition(spots[i].x, y - 0.1f, spots[i].y); // (sunk a little: the ground is not flat)
    tree->setYaw(r1 * 6.2831853f * 7.0f);
    tree->setScale(scale);
    add(tree);
    trees.push_back(vec4(spots[i].x, y, spots[i].y, heights[model] * scale));
  }
  buildCanopy(trees);
}

void PineForestStage::buildCanopy(const std::vector<vec4> &trees) {
  const int N = CANOPY_TEXELS;
  const float size = 2.0f * HALF_SIZE, texel = size / N;
  const float planes[3] = {CANOPY_LOW, CANOPY_MID, CANOPY_HIGH};
  std::vector<float> open(N * N * 3, 1.0f); // 1 = open sky
  for (const vec4 &t : trees) {
    float h = t.w, base = CROWN_BASE * h;
    for (int k = 0; k < 3; k++) {
      // the crown's radius at this plane: the cone of foliage, from CROWN_BASE up to the top
      float y = GROUND_Y + planes[k] - t.y; // (the plane's height over this tree's foot)
      if (y < base || y > h)
        continue;
      float radius = CROWN_RADIUS * h * (1.0f - (y - base) / (h - base)) + 0.4f;
      int x0 = std::max(0, (int)((t.x - radius + HALF_SIZE) / texel));
      int x1 = std::min(N - 1, (int)((t.x + radius + HALF_SIZE) / texel));
      int z0 = std::max(0, (int)((t.z - radius + HALF_SIZE) / texel));
      int z1 = std::min(N - 1, (int)((t.z + radius + HALF_SIZE) / texel));
      for (int zi = z0; zi <= z1; zi++)
        for (int xi = x0; xi <= x1; xi++) {
          float wx = -HALF_SIZE + (xi + 0.5f) * texel, wz = -HALF_SIZE + (zi + 0.5f) * texel;
          float d = length(vec2(wx - t.x, wz - t.z));
          if (d > radius)
            continue;
          // dense in the middle, ragged at the edge, with gaps between the needles
          float edge = 1.0f - smooth01(radius * 0.6f, radius, d);
          float n = 0.65f * valueNoise(wx + 37.0f * k, wz, 0.45f) + 0.35f * valueNoise(wx, wz - 19.0f * k, 1.6f);
          float leaves = edge * (1.0f - smooth01(0.40f, 0.58f, n));
          float &o = open[(zi * N + xi) * 3 + k];
          o *= 1.0f - LEAF_DENSITY * leaves;
        }
    }
  }
  std::vector<unsigned char> bytes(open.size());
  for (size_t i = 0; i < open.size(); i++)
    bytes[i] = (unsigned char)(clamp(open[i], 0.0f, 1.0f) * 255.0f + 0.5f);
  glGenTextures(1, &canopyTexture);
  glBindTexture(GL_TEXTURE_2D, canopyTexture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, N, N, 0, GL_RGB, GL_UNSIGNED_BYTE, bytes.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  environment.canopyMask = canopyTexture;
  environment.canopyMin = vec2(-HALF_SIZE);
  environment.canopySize = size;
  environment.canopyHeights = vec3(GROUND_Y + CANOPY_LOW, GROUND_Y + CANOPY_MID, GROUND_Y + CANOPY_HIGH);
}

// The day: as in the desert (TestStage::onTimeChanged), without the music
void PineForestStage::onTimeChanged() {
  float angle = (getTimeOfDay() - 6.0f) / 24.0f * 6.2831853f;
  vec3 sun = normalize(vec3(std::cos(angle), std::sin(angle), -0.3f));
  float h = sun.y;
  float day = smooth01(-0.1f, 0.3f, h);
  float dusk = smooth01(-0.2f, 0.0f, h) * (1.0f - smooth01(0.05f, 0.35f, h));
  vec3 horizon = mix(vec3(0.035f, 0.055f, 0.11f), vec3(0.50f, 0.66f, 0.80f), day);
  horizon = mix(horizon, vec3(0.95f, 0.52f, 0.30f), dusk * 0.85f);
  vec3 zenith = mix(vec3(0.003f, 0.007f, 0.028f), vec3(0.16f, 0.38f, 0.78f), day);
  zenith = mix(zenith, vec3(0.25f, 0.22f, 0.45f), dusk * 0.5f);
  environment.horizon = horizon;
  environment.skyZenith = zenith;
  environment.sunDir = sun;
  environment.starAlpha = 1.0f - smooth01(-0.2f, 0.0f, h);
  vec3 sunColor = mix(vec3(1.0f, 0.55f, 0.25f), vec3(0.85f, 0.83f, 0.78f), smooth01(0.0f, 0.45f, h));
  float sunPower = smooth01(-0.05f, 0.25f, h);
  environment.lightColor = sunColor * sunPower + NIGHT_LIGHT;
  float sunShare = sunPower / (sunPower + length(NIGHT_LIGHT));
  environment.lightDir = normalize(mix(vec3(0.0f, 1.0f, 0.0f), sun, sunShare));
}

void PineForestStage::enterRV() {
  if (inVehicle)
    return;
  inVehicle = true;
  walker->control(vec2(0.0f), 0.0f, 0.0f);
  walker->setVelocity(vec3(0.0f));
  walker->setGravity(0.0f);
  walker->setCollidable(false);
  walker->setVisible(false);
  vec3 seat = rv->seatPosition();
  walker->setPosition(seat.x, seat.y, seat.z);
  rv->setOccupied(true);
  setPlayer(rv, CAR_CAMERA_DISTANCE, CAR_CAMERA_HEIGHT, rv->headingYaw());
}

void PineForestStage::leaveVehicle() {
  if (!inVehicle)
    return;
  inVehicle = false;
  rv->control(vec2(0.0f), 0.0f, 0.0f);
  rv->setOccupied(false);
  vec3 door = rv->doorPosition(1.5f);
  walker->setPosition(door.x, groundAt(door.x, door.z), door.z);
  walker->setVelocity(vec3(0.0f));
  walker->setGravity(25.0f);
  walker->setCollidable(true);
  setPlayer(walker, 0.0f, EYE_HEIGHT, rv->doorYaw());
}

void PineForestStage::apply(DynamicGameObject &object, double dt) {
  if (inVehicle && &object == walker.get()) { // the penguin rides in the RV
    vec3 seat = rv->seatPosition();
    object.setPosition(seat.x, seat.y, seat.z);
    object.setVelocity(vec3(0.0f));
    return;
  }
  collideWithFloor(object, dt);
}

void PineForestStage::toggleHeadlights() {
  if (inVehicle)
    rv->toggleHeadlights();
  else
    walker->toggleFlashlight();
}
void PineForestStage::toggleEngine() {
  if (inVehicle)
    rv->toggleEngine();
}
void PineForestStage::toggleHandbrake() {
  if (inVehicle)
    rv->toggleHandbrake();
}
void PineForestStage::toggleVehicleCamera() {
  if (inVehicle)
    rv->toggleCameraView();
}

void PineForestStage::getSpotLights(std::vector<SpotLight> &lights) const {
  if (!inVehicle)
    walker->getFlashlight(lights);
  rv->getHeadlights(lights);
  rv->getDashboardLights(lights);
}
