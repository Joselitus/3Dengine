#include "TerrainPaint.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "Gfx.h"
#include "Stage.h"
#include "myopengl.h"

using namespace std;
using namespace glm;

constexpr int TerrainPaint::CHUNK;
constexpr unsigned char TerrainPaint::NONE;
constexpr float TerrainPaint::LIFT;

namespace {
// The texture of each material, and how many metres one repeat of it covers
const char *TEXTURES[] = {"../assets/desert/sand.jpg", "../assets/route66/r66_road.jpg",
                          "../assets/forest/forest_floor.jpg"};
const float TILE[] = {6.0f, 4.0f, 4.0f};

unsigned int textureOf(int material) {
  static map<int, unsigned int> loaded;
  auto found = loaded.find(material);
  if (found == loaded.end())
    found = loaded.insert({material, TextureFromFile(TEXTURES[material], ".")}).first;
  return found->second;
}
} // namespace

TerrainPaint::TerrainPaint(Stage &stage) : stage(stage) {
  Stage::TerrainGrid g = stage.terrainGrid();
  cellsX = g.nx - 1;
  cellsZ = g.nz - 1;
  shown.assign((size_t)cellsX * cellsZ, NONE);
  origin = vec3(g.x0 + 0.5f * g.dx * cellsX, 0.0f, g.z0 + 0.5f * g.dz * cellsZ);
  // Which way the floor's own triangles cut a square: from the first triangle that has two corners
  // of a square for its edge
  mainDiagonal = true;
  const Model &floor = *stage.getFloorModel();
  for (const Mesh &mesh : floor.meshes) {
    const vector<Vertex> &vs = mesh.getVertices();
    const vector<unsigned int> &is = mesh.getIndices();
    bool found = false;
    for (size_t i = 0; i + 2 < is.size() && !found; i += 3)
      for (int a = 0; a < 3 && !found; a++) {
        const vec3 &p = vs[is[i + a]].Position, &q = vs[is[i + (a + 1) % 3]].Position;
        float sx = (q.x - p.x) / g.dx, sz = (q.z - p.z) / g.dz;
        if (std::fabs(std::fabs(sx) - 1.0f) < 0.01f && std::fabs(std::fabs(sz) - 1.0f) < 0.01f) {
          mainDiagonal = sx * sz > 0.0f;
          found = true;
        }
      }
    if (found)
      break;
  }
  object = make_shared<GameObject>(model);
  object->setPosition(origin.x, origin.y, origin.z);
  object->setCollidable(false); // (before it is added)
  object->setCullRadius(1.0e6f);
  stage.add(object);
}

void TerrainPaint::touch(int mesh, size_t first, size_t last) {
  if ((size_t)mesh >= touched.size())
    touched.resize(mesh + 1, {SIZE_MAX, 0});
  pair<size_t, size_t> &range = touched[mesh];
  range.first = std::min(range.first, first);
  range.second = std::max(range.second, last);
}

int TerrainPaint::meshFor(Chunk &chunk, int chunkX, int chunkZ, FloorMaterial material) {
  int &index = chunk.mesh[(int)material];
  if (index >= 0)
    return index;
  // Every square of the block, four vertices and two triangles each, all collapsed for now
  vector<Vertex> vertices((size_t)CHUNK * CHUNK * 4);
  vector<unsigned int> indices;
  for (int c = 0; c < CHUNK * CHUNK; c++) {
    unsigned int v = (unsigned int)c * 4;
    // corners: 0 (ix, iz), 1 (ix + 1, iz), 2 (ix, iz + 1), 3 (ix + 1, iz + 1)
    if (mainDiagonal) {
      for (unsigned int i : {0u, 2u, 3u, 0u, 3u, 1u})
        indices.push_back(v + i);
    } else {
      for (unsigned int i : {0u, 2u, 1u, 1u, 2u, 3u})
        indices.push_back(v + i);
    }
  }
  for (Vertex &v : vertices) {
    v.Position = vec3(0.0f);
    v.Normal = vec3(0.0f, 1.0f, 0.0f);
    v.TexCoords = vec2(0.0f);
  }
  Texture texture;
  texture.id = textureOf((int)material);
  texture.type = "texture_diffuse";
  texture.path = TEXTURES[(int)material];
  model->meshes.push_back(Mesh(vertices, indices, {texture}));
  model->meshes.back().setMaterialName("painted");
  index = (int)model->meshes.size() - 1;
  (void)chunkX;
  (void)chunkZ;
  return index;
}

void TerrainPaint::writeCell(const Chunk &chunk, int cx, int cz, FloorMaterial material, bool show) {
  Stage::TerrainGrid g = stage.terrainGrid();
  int mesh = chunk.mesh[(int)material];
  if (mesh < 0)
    return;
  size_t first = (size_t)(((cz % CHUNK) * CHUNK) + (cx % CHUNK)) * 4;
  vector<Vertex> &vs = model->meshes[mesh].editableVertices();
  const int cornerX[4] = {0, 1, 0, 1}, cornerZ[4] = {0, 0, 1, 1};
  auto height = [&](int ix, int iz) {
    return stage.terrainHeight(std::min(std::max(ix, 0), g.nx - 1), std::min(std::max(iz, 0), g.nz - 1));
  };
  for (int k = 0; k < 4; k++) {
    Vertex &v = vs[first + k];
    if (!show) {
      v.Position = vec3(0.0f, 0.0f, 0.0f);
      continue;
    }
    int ix = cx + cornerX[k], iz = cz + cornerZ[k];
    float x = g.x0 + ix * g.dx, z = g.z0 + iz * g.dz;
    v.Position = vec3(x - origin.x, height(ix, iz) + LIFT, z - origin.z);
    v.Normal = normalize(vec3((height(ix - 1, iz) - height(ix + 1, iz)) / (2.0f * g.dx), 1.0f,
                              (height(ix, iz - 1) - height(ix, iz + 1)) / (2.0f * g.dz)));
    v.TexCoords = vec2(x, z) / TILE[(int)material];
  }
  touch(mesh, first, first + 3);
}

void TerrainPaint::set(int cx, int cz, FloorMaterial material, bool show) {
  if (cx < 0 || cz < 0 || cx >= cellsX || cz >= cellsZ)
    return;
  unsigned char &current = shown[(size_t)cz * cellsX + cx];
  long long key = ((long long)(cz / CHUNK) << 32) ^ (unsigned int)(cx / CHUNK);
  // Whatever was painted there before goes away
  if (current != NONE) {
    auto old = chunks.find(key);
    if (old != chunks.end())
      writeCell(old->second, cx, cz, (FloorMaterial)current, false);
    current = NONE;
  }
  if (!show)
    return;
  Chunk &chunk = chunks[key];
  meshFor(chunk, cx / CHUNK, cz / CHUNK, material);
  current = (unsigned char)material;
  writeCell(chunk, cx, cz, material, true);
}

void TerrainPaint::heightsChanged(int ix0, int iz0, int ix1, int iz1) {
  for (int cz = std::max(iz0 - 1, 0); cz <= std::min(iz1, cellsZ - 1); cz++)
    for (int cx = std::max(ix0 - 1, 0); cx <= std::min(ix1, cellsX - 1); cx++) {
      unsigned char m = shown[(size_t)cz * cellsX + cx];
      if (m == NONE)
        continue;
      long long key = ((long long)(cz / CHUNK) << 32) ^ (unsigned int)(cx / CHUNK);
      writeCell(chunks[key], cx, cz, (FloorMaterial)m, true);
    }
}

void TerrainPaint::flush() {
  for (size_t m = 0; m < touched.size(); m++)
    if (touched[m].first != SIZE_MAX)
      model->meshes[m].refreshVertices(touched[m].first, touched[m].second - touched[m].first + 1);
  touched.clear();
}
