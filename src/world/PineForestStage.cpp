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
const float MORNING = 10.0f;           // it starts in the morning: long, slanting shadows
// The overhead canopy (buildCanopy): how much light a patch of leaves stops in one plane (three
// planes overlap: where they all have leaves it is darkest), how wide its crowns are (m), and
// where the gaps between them open (0..1: higher, more gaps)
const float LEAF_DENSITY = 0.6f;
const float CROWN_SIZE = 9.0f;
const float CROWN_GAP = 0.47f;

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

PineForestStage::PineForestStage(FloorMode mode, SoundEngine &sound) : VehicleStage(mode) {
  // Only the wind (the desert's music would not fit)
  loadAmbience("../assets/music/wind.wav");
  groundFallback = GROUND_Y;

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

  // The RV in the clearing, facing +z (its door, on +x, towards the start), and the player on
  // foot beside it, in first person
  createRV(sound, 0.0f, 0.0f, 0.0f);
  createWalker(3.0f, 4.0f);
  cameraDistance = 0.0f;
  cameraHeight = EYE_HEIGHT;

  // Bob's ship lands in the clearing, away from the RV, its ramp towards the start
  vec3 landing(-9.0f, 0.0f, -9.0f);
  landing.y = groundAt(landing.x, landing.z);
  createAlienVisit(landing, std::atan2(3.0f - landing.x, 4.0f - landing.z));

  // The day as in the desert (VehicleStage), from the morning, with no dunes on the sky
  startDay();
  environment.skyDunes = false;
  setTimeOfDay(MORNING);
}

PineForestStage::~PineForestStage() {
  if (canopyTexture)
    glDeleteTextures(1, &canopyTexture);
}

// Pines all over, at least TREE_SPACING apart (Poisson disk), none in the clearing nor at the
// edge; each one of the three models, turned and sized at random
void PineForestStage::plantTrees() {
  std::shared_ptr<Model> models[3] = {loadModel("../assets/pine_forest/pine_0.obj"),
                                      loadModel("../assets/pine_forest/pine_1.obj"),
                                      loadModel("../assets/pine_forest/pine_2.obj")};
  vec2 lo(-HALF_SIZE + TREE_MARGIN), hi(HALF_SIZE - TREE_MARGIN);
  std::vector<vec2> spots = poissonDisk(lo, hi, TREE_SPACING, 20261007u, 30,
                                        [](const vec2 &p) { return length(p) > CLEARING; });
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
  }
  buildCanopy();
}

// The canopy covers the whole map: the crowns of trees far taller than the pines, high above
// them, that shade everything (the pines too) except where the sun gets through, in the gaps
// between those crowns and between their leaves (see the class comment)
void PineForestStage::buildCanopy() {
  const int N = CANOPY_TEXELS;
  const float size = 2.0f * HALF_SIZE, texel = size / N;
  std::vector<float> open(N * N * 3, 1.0f); // 1 = open sky
  for (int zi = 0; zi < N; zi++)
    for (int xi = 0; xi < N; xi++) {
      float wx = -HALF_SIZE + (xi + 0.5f) * texel, wz = -HALF_SIZE + (zi + 0.5f) * texel;
      for (int k = 0; k < 3; k++) {
        // the crowns: blobs CROWN_SIZE wide, a different pattern in each plane
        float crowns = 0.7f * valueNoise(wx + 91.0f * k, wz - 57.0f * k, CROWN_SIZE) +
                       0.3f * valueNoise(wx - 23.0f * k, wz + 41.0f * k, CROWN_SIZE * 0.45f);
        float cover = smooth01(CROWN_GAP - 0.08f, CROWN_GAP + 0.08f, crowns);
        // the leaves: small gaps inside each crown
        float n = 0.6f * valueNoise(wx + 37.0f * k, wz, 0.5f) + 0.4f * valueNoise(wx, wz - 19.0f * k, 1.6f);
        float leaves = cover * (1.0f - smooth01(0.50f, 0.66f, n));
        open[(zi * N + xi) * 3 + k] = 1.0f - LEAF_DENSITY * leaves;
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
