#include "RoadMesh.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "Gfx.h"
#include "Stage.h"
#include "myopengl.h"

using namespace std;
using namespace glm;

constexpr int RoadMesh::COLUMNS;
constexpr float RoadMesh::STEP;
constexpr float RoadMesh::LIFT;

namespace {
// The texture of each surface and how many road widths long one repeat of it is (the textures
// are drawn across the width: edge lines and dashes of the asphalt, the ruts of the dirt)
const char *TEXTURES[] = {"../assets/desert/road.jpg", "../assets/desert/dirt_road.png"};
const float REPEAT_LENGTH[] = {1.0f, 2.0f};

unsigned int textureOf(int type) {
  static map<int, unsigned int> loaded;
  auto found = loaded.find(type);
  if (found == loaded.end())
    found = loaded.insert({type, TextureFromFile(TEXTURES[type], ".")}).first;
  return found->second;
}
} // namespace

RoadMesh::RoadMesh(Stage &stage) {
  makeMesh(64, Road::Asphalt);
  object = make_shared<GameObject>(model);
  object->setCollidable(false); // (before it is added)
  object->setCullRadius(1.0e6f);
  stage.add(object);
}

void RoadMesh::makeMesh(size_t rows, int type) {
  vector<Vertex> vertices(rows * COLUMNS);
  for (Vertex &v : vertices) {
    v.Position = vec3(0.0f);
    v.Normal = vec3(0.0f, 1.0f, 0.0f);
    v.TexCoords = vec2(0.0f);
  }
  vector<unsigned int> indices;
  for (size_t r = 0; r + 1 < rows; r++)
    for (int c = 0; c + 1 < COLUMNS; c++) {
      unsigned int a = (unsigned int)(r * COLUMNS + c), b = a + 1, n = a + COLUMNS, d = n + 1;
      for (unsigned int i : {a, b, n, b, d, n})
        indices.push_back(i);
    }
  Texture texture;
  texture.id = textureOf(type);
  texture.type = "texture_diffuse";
  texture.path = TEXTURES[type];
  Mesh mesh(vertices, indices, {texture});
  mesh.setMaterialName("road");
  if (model->meshes.empty())
    model->meshes.push_back(mesh);
  else
    model->meshes[0] = mesh;
  capacity = rows;
  textureType = type;
}

void RoadMesh::update(const Stage &stage, const Road &road, int order) {
  vector<RoadSample> samples = sampleRoad(road, STEP);
  if (samples.size() < 2) {
    object->setVisible(false);
    return;
  }
  object->setVisible(true);
  int type = clamp(road.type, 0, 1);
  if (samples.size() > capacity || type != textureType) {
    size_t rows = capacity;
    while (rows < samples.size())
      rows *= 2;
    makeMesh(rows, type);
  }
  vector<Vertex> &vertices = model->meshes[0].editableVertices();
  float length = samples.back().distance;
  float repeat = road.width * REPEAT_LENGTH[type];
  if (road.closed) // a whole number of repeats, so that the pattern meets itself where it closes
    repeat = length / std::max(1.0f, std::round(length / repeat));
  float lift = LIFT + 0.01f * order;
  float lastHeight = 0.0f;
  for (size_t r = 0; r < capacity; r++) {
    const RoadSample &s = samples[std::min(r, samples.size() - 1)]; // (rows left over collapse onto the end)
    vec2 across(-s.tangent.y, s.tangent.x);
    for (int c = 0; c < COLUMNS; c++) {
      float u = (float)c / (COLUMNS - 1);
      vec2 at = s.position + across * ((u - 0.5f) * road.width);
      float height = lastHeight;
      vec3 normal(0.0f, 1.0f, 0.0f);
      if (stage.floorAt(at.x, at.y, height, &normal))
        lastHeight = height;
      Vertex &v = vertices[r * COLUMNS + c];
      v.Position = vec3(at.x, height + lift, at.y);
      v.Normal = normal;
      v.TexCoords = vec2(u, s.distance / repeat);
    }
  }
  model->meshes[0].refreshVertices(0, vertices.size());
}
