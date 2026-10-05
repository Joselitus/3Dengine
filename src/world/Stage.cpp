#include "Stage.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace std;
using namespace glm;

// Distance below which the floor still holds an object that is going down
// (so it follows slopes instead of hopping), and how much it may step up
#define SNAP_DISTANCE 0.3f
#define STEP_HEIGHT 0.6f
// How far above a point keepAboveFloor() still looks for the floor (DownwardRay)
#define CAMERA_REACH 3.0f

shared_ptr<Model> Stage::loadModel(const string &path) {
  shared_ptr<Model> &model = models[path];
  if (!model)
    model = make_shared<Model>(path.c_str());
  return model;
}

bool Stage::loadMusic(const string &path, bool loop, float volume) {
  auto clip = make_shared<AudioClip>();
  if (!clip->loadWavFile(path)) {
    cerr << "Stage: could not load the music '" << path << "'" << endl;
    music = nullptr;
    return false;
  }
  setMusic(clip, loop, volume);
  return true;
}

bool Stage::loadAmbience(const string &path, float volume) {
  auto clip = make_shared<AudioClip>();
  if (!clip->loadWavFile(path)) {
    cerr << "Stage: could not load the ambient sound '" << path << "'" << endl;
    ambience = nullptr;
    return false;
  }
  setAmbience(clip, volume);
  return true;
}

void Stage::addEmitter(shared_ptr<ParticleEmitter> emitter) {
  // The particles that reach the floor go (there is no floor outside it)
  emitter->setGround([this](float x, float z, float &height) {
    return floorAt(x, z, height);
  });
  emitters.push_back(emitter);
}

shared_ptr<GameObject> Stage::add(shared_ptr<GameObject> object) {
  objects.push_back(object);
  registerBody(object.get(), nullptr);
  return object;
}

shared_ptr<DynamicGameObject>
Stage::addDynamic(shared_ptr<DynamicGameObject> object) {
  dynamicObjects.push_back(object);
  registerBody(object.get(), object.get());
  return object;
}

void Stage::setTimeOfDay(float hours) {
  timeOfDay = std::fmod(hours, 24.0f);
  if (timeOfDay < 0.0f)
    timeOfDay += 24.0f;
  onTimeChanged();
}

void Stage::update(double dt) {
  if (dayDuration > 0.0f) {
    setTimeOfDay(timeOfDay + (float)dt * 24.0f / dayDuration);
  }
  for (auto &object : objects)
    object->update(dt);
  for (auto &object : dynamicObjects) {
    object->update(dt);
    apply(*object, dt);
    collideShapeWithFloor(*object);
  }
  resolveCollisions();
  // The collisions may have pushed them into the floor
  for (auto &object : dynamicObjects)
    collideShapeWithFloor(*object);
  // The emitters, once their owners have moved them
  for (auto &emitter : emitters)
    emitter->update(dt);
}

void Stage::Draw(Shader *shader, double time) {
  shader->setFloat("breathTime", (float)time);
  for (auto &object : objects)
    object->Draw(shader);
  for (auto &object : dynamicObjects)
    object->Draw(shader);
  // translucent meshes (windows) last, over everything opaque
  for (auto &object : objects)
    object->DrawTransparent(shader);
  for (auto &object : dynamicObjects)
    object->DrawTransparent(shader);
}

// ------------------------------------------------------------------- floor
static vec3 triangleNormal(const vec3 &a, const vec3 &b, const vec3 &c) {
  vec3 n = cross(b - a, c - a);
  float len = length(n);
  if (len == 0.0f)
    return vec3(0.0f, 1.0f, 0.0f);
  n /= len;
  return n.y < 0.0f ? -n : n;
}

bool Stage::setFloor(shared_ptr<Model> mesh, const vec3 &position,
                     shared_ptr<const MaterialMap> materials) {
  if (floor_mesh) {
    cerr << "Stage: the floor can only be set once" << endl;
    return false;
  }
  if (floorMode == FloorMode::HeightField && !materials) {
    cerr << "Stage: a HeightField floor needs a material map" << endl;
    return false;
  }
  vector<vec3> verts;
  vector<unsigned int> indices;
  vector<unsigned char> meshMaterials; // one per triangle, from the mesh
  for (const Mesh &m : mesh->meshes) {
    unsigned int base = verts.size();
    for (const Vertex &v : m.getVertices())
      verts.push_back(v.Position + position);
    for (unsigned int i : m.getIndices())
      indices.push_back(base + i);
    unsigned char material =
        (unsigned char)floorMaterialFromName(m.getMaterialName());
    meshMaterials.insert(meshMaterials.end(), m.getIndices().size() / 3,
                         material);
  }
  if (verts.empty() || indices.size() < 3) {
    cerr << "Stage: the floor mesh is empty" << endl;
    return false;
  }

  minX = minZ = numeric_limits<float>::max();
  maxX = maxZ = -numeric_limits<float>::max();
  for (const vec3 &v : verts) {
    minX = std::min(minX, v.x);
    maxX = std::max(maxX, v.x);
    minZ = std::min(minZ, v.z);
    maxZ = std::max(maxZ, v.z);
  }

  if (floorMode == FloorMode::HeightField) {
    if (!buildHeightField(verts))
      return false;
  } else {
    triVerts.clear();
    for (unsigned int i : indices)
      triVerts.push_back(verts[i]);
    triMaterials = meshMaterials;
    buildTriangleGrid();
  }
  floorMaterials = materials;
  floor_mesh = mesh;
  return true;
}

FloorMaterial Stage::materialAt(float x, float z) const {
  if (!floor_mesh || x < minX || x > maxX || z < minZ || z > maxZ)
    return FloorMaterial::Sand;
  if (floorMaterials)
    return floorMaterials->at((x - minX) / std::max(maxX - minX, 1e-6f),
                              (z - minZ) / std::max(maxZ - minZ, 1e-6f));
  // No map (a DownwardRay floor): the material of the triangle under the point
  float height;
  int triangle = -1;
  if (rayAt(x, z, 1e30f, height, nullptr, &triangle) && triangle >= 0 &&
      (size_t)triangle < triMaterials.size())
    return (FloorMaterial)triMaterials[triangle];
  return FloorMaterial::Sand;
}

// The grid is read off the vertices: every distinct x and z is a grid line
bool Stage::buildHeightField(const vector<vec3> &verts) {
  const float eps = 1e-3f;
  vector<float> xs, zs;
  for (const vec3 &v : verts) {
    xs.push_back(v.x);
    zs.push_back(v.z);
  }
  for (vector<float> *a : {&xs, &zs}) {
    sort(a->begin(), a->end());
    a->erase(unique(a->begin(), a->end(),
                    [&](float p, float q) { return q - p < eps; }),
             a->end());
  }
  nx = xs.size();
  nz = zs.size();
  if (nx < 2 || nz < 2) {
    cerr << "Stage: the floor is not a grid (HeightField)" << endl;
    return false;
  }
  x0 = xs.front();
  z0 = zs.front();
  dx = (xs.back() - x0) / (nx - 1);
  dz = (zs.back() - z0) / (nz - 1);
  for (int i = 0; i < nx; i++)
    if (fabs(xs[i] - (x0 + i * dx)) > 0.01f * dx) {
      cerr << "Stage: the floor is not a regular grid in x (HeightField)"
           << endl;
      return false;
    }
  for (int i = 0; i < nz; i++)
    if (fabs(zs[i] - (z0 + i * dz)) > 0.01f * dz) {
      cerr << "Stage: the floor is not a regular grid in z (HeightField)"
           << endl;
      return false;
    }

  heights.assign((size_t)nx * nz, numeric_limits<float>::quiet_NaN());
  for (const vec3 &v : verts) {
    int ix = (int)lround((v.x - x0) / dx);
    int iz = (int)lround((v.z - z0) / dz);
    heights[(size_t)iz * nx + ix] = v.y;
  }
  for (float h : heights)
    if (std::isnan(h)) {
      cerr << "Stage: the floor grid has holes (HeightField)" << endl;
      return false;
    }
  return true;
}

void Stage::buildTriangleGrid() {
  size_t tris = triVerts.size() / 3;
  // about two triangles per cell
  float area = std::max((maxX - minX) * (maxZ - minZ), 1e-3f);
  cellSize = std::max(2.0f * sqrt(area / tris), 1e-3f);
  cellsX = (int)ceil((maxX - minX) / cellSize) + 1;
  cellsZ = (int)ceil((maxZ - minZ) / cellSize) + 1;
  cells.assign((size_t)cellsX * cellsZ, vector<unsigned int>());
  for (size_t t = 0; t < tris; t++) {
    const vec3 *p = &triVerts[3 * t];
    float lx = std::min(p[0].x, std::min(p[1].x, p[2].x));
    float hx = std::max(p[0].x, std::max(p[1].x, p[2].x));
    float lz = std::min(p[0].z, std::min(p[1].z, p[2].z));
    float hz = std::max(p[0].z, std::max(p[1].z, p[2].z));
    int cx0 = (int)floor((lx - minX) / cellSize);
    int cx1 = (int)floor((hx - minX) / cellSize);
    int cz0 = (int)floor((lz - minZ) / cellSize);
    int cz1 = (int)floor((hz - minZ) / cellSize);
    for (int cz = cz0; cz <= cz1; cz++)
      for (int cx = cx0; cx <= cx1; cx++)
        cells[(size_t)cz * cellsX + cx].push_back(t);
  }
}

// Same diagonal as the terrain generator: from (i+1, j) to (i, j+1)
bool Stage::heightFieldAt(float x, float z, float &height, vec3 *normal) const {
  float gx = (x - x0) / dx, gz = (z - z0) / dz;
  if (!(gx >= 0 && gz >= 0 && gx <= nx - 1 && gz <= nz - 1))
    return false; // outside (or not a number)
  int ix = std::min((int)gx, nx - 2), iz = std::min((int)gz, nz - 2);
  float fx = gx - ix, fz = gz - iz;
  float ha = heights[(size_t)iz * nx + ix];
  float hb = heights[(size_t)iz * nx + ix + 1];
  float hc = heights[(size_t)(iz + 1) * nx + ix];
  float hd = heights[(size_t)(iz + 1) * nx + ix + 1];
  vec3 a(0, ha, 0), b(dx, hb, 0), c(0, hc, dz), d(dx, hd, dz);
  if (fx + fz <= 1.0f) {
    height = ha + fx * (hb - ha) + fz * (hc - ha);
    if (normal)
      *normal = triangleNormal(a, c, b);
  } else {
    height = hd + (1 - fx) * (hc - hd) + (1 - fz) * (hb - hd);
    if (normal)
      *normal = triangleNormal(b, c, d);
  }
  return true;
}

// Vertical ray: a triangle is hit if (x, z) is inside its x/z footprint
bool Stage::rayAt(float x, float z, float maxY, float &height, vec3 *normal,
                  int *triangle) const {
  int cx = (int)floor((x - minX) / cellSize);
  int cz = (int)floor((z - minZ) / cellSize);
  if (x < minX || x > maxX || z < minZ || z > maxZ || cx < 0 || cz < 0 ||
      cx >= cellsX || cz >= cellsZ)
    return false;
  bool hit = false;
  const float eps = -1e-5f;
  for (unsigned int t : cells[(size_t)cz * cellsX + cx]) {
    const vec3 &a = triVerts[3 * t], &b = triVerts[3 * t + 1],
               &c = triVerts[3 * t + 2];
    float det = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
    if (fabs(det) < 1e-9f)
      continue; // vertical triangle
    float l1 = ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) / det;
    float l2 = ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) / det;
    float l3 = 1.0f - l1 - l2;
    if (l1 < eps || l2 < eps || l3 < eps)
      continue;
    float y = l1 * a.y + l2 * b.y + l3 * c.y;
    if (y > maxY || (hit && y <= height))
      continue;
    hit = true;
    height = y;
    if (normal)
      *normal = triangleNormal(a, b, c);
    if (triangle)
      *triangle = (int)t;
  }
  return hit;
}

bool Stage::floorAt(float x, float z, float &height, vec3 *normal,
                    float maxY) const {
  if (!floor_mesh)
    return false;
  if (floorMode == FloorMode::HeightField)
    return heightFieldAt(x, z, height, normal); // grid is in world space
  return rayAt(x, z, maxY, height, normal);
}

void Stage::keepInsideFloor(vec3 &p, vec3 &v, float margin) const {
  if (!floor_mesh)
    return;
  float cx = glm::clamp(p.x, minX + margin, maxX - margin);
  float cz = glm::clamp(p.z, minZ + margin, maxZ - margin);
  if (cx != p.x) v.x = 0.0f;
  if (cz != p.z) v.z = 0.0f;
  p.x = cx;
  p.z = cz;
}

bool Stage::keepAboveFloor(vec3 &point, float radius) const {
  float h;
  vec3 normal(0.0f, 1.0f, 0.0f);
  if (!floorAt(point.x, point.z, h, &normal, point.y + CAMERA_REACH))
    return false;
  // On a slope the sphere touches the floor higher than `radius` above it
  float needed = h + radius / glm::max(normal.y, 0.5f);
  if (point.y >= needed)
    return false;
  point.y = needed;
  return true;
}

void Stage::collideWithFloor(DynamicGameObject &object, double dt) const {
  if (object.contactFloor(*this, dt))
    return;
  if (!floor_mesh)
    return;
  vec3 p = object.getPosition();
  vec3 v = object.getVelocity();
  keepInsideFloor(p, v); // stay inside the floor

  float h;
  bool onFloor = false;
  if (floorAt(p.x, p.z, h, nullptr, p.y + STEP_HEIGHT)) {
    // below the floor, or going down and close enough to be held by it
    if (p.y <= h || (object.isGrounded() && v.y <= 0.0f && p.y - h < SNAP_DISTANCE)) {
      p.y = h;
      if (v.y < 0.0f)
        v.y = 0.0f;
      onFloor = true;
    }
  }
  object.setPosition(p.x, p.y, p.z);
  object.setVelocity(v);
  object.setGrounded(onFloor);
}

// -------------------------------------------------------------- collisions
// An object can't cover more cells than this: it is surely a mistake (e.g. the
// floor itself, which should not be collidable)
#define MAX_CELLS_PER_OBJECT 1024

static long long cellKey(int x, int z) {
  return ((long long)(unsigned int)x << 32) | (unsigned int)z;
}

bool Stage::cellRange(const GameObject &object, int &x0, int &z0, int &x1,
                      int &z1) const {
  vec3 min, max;
  object.getShape().bounds(object.getPose(), min, max);
  x0 = (int)floor(min.x / gridCellSize);
  z0 = (int)floor(min.z / gridCellSize);
  x1 = (int)floor(max.x / gridCellSize);
  z1 = (int)floor(max.z / gridCellSize);
  return (long long)(x1 - x0 + 1) * (z1 - z0 + 1) <= MAX_CELLS_PER_OBJECT;
}

void Stage::registerBody(GameObject *object, DynamicGameObject *dynamic) {
  int index = (int)bodies.size();
  bodies.push_back({object, dynamic});
  if (dynamic || !object->isCollidable())
    return; // the dynamic ones are placed on the grid every update
  placeStatic(index);
}

void Stage::placeStatic(int index) {
  int x0, z0, x1, z1;
  if (!cellRange(*bodies[index].object, x0, z0, x1, z1)) {
    cerr << "Stage: a static object covers too many collision cells, it is "
            "ignored (setCollidable(false) if it is the floor)" << endl;
    return;
  }
  for (int z = z0; z <= z1; z++)
    for (int x = x0; x <= x1; x++)
      gridCells[cellKey(x, z)].statics.push_back(index);
}

void Stage::rebuildStaticGrid() {
  for (auto &cell : gridCells)
    cell.second.statics.clear();
  for (size_t i = 0; i < bodies.size(); i++)
    if (!bodies[i].dynamic && bodies[i].object->isCollidable())
      placeStatic((int)i);
}

void Stage::relocate(GameObject &object, const vec3 &position) {
  object.teleport(position);
  staticMoved(object);
}

void Stage::turn(GameObject &object, float radians) {
  object.turn(radians);
  staticMoved(object);
}

void Stage::staticMoved(const GameObject &object) {
  // A static one (or its parts that are objects of their own, like the post
  // of a satellite) may now be in other cells: they are few, place them all
  // again
  for (const Body &body : bodies)
    if (body.object == &object && !body.dynamic) {
      rebuildStaticGrid();
      return;
    }
}

void Stage::resolveCollisions() {
  // Where the dynamic objects are now
  for (auto &cell : gridCells)
    cell.second.dynamics.clear();
  for (size_t i = 0; i < bodies.size(); i++) {
    if (!bodies[i].dynamic || !bodies[i].object->isCollidable())
      continue;
    int x0, z0, x1, z1;
    if (!cellRange(*bodies[i].object, x0, z0, x1, z1))
      continue;
    for (int z = z0; z <= z1; z++)
      for (int x = x0; x <= x1; x++)
        gridCells[cellKey(x, z)].dynamics.push_back((int)i);
  }

  // Only the objects in the same cell are tested. A couple of passes, because
  // pushing one object out of another can push it into a third.
  std::vector<long long> tested;
  shapeTests = 0;
  for (int pass = 0; pass < 2; pass++) {
    tested.clear();
    for (auto &entry : gridCells) {
      const Cell &cell = entry.second;
      for (size_t a = 0; a < cell.dynamics.size(); a++) {
        for (size_t b = a + 1; b < cell.dynamics.size(); b++)
          collideBodies(cell.dynamics[a], cell.dynamics[b], tested);
        for (int s : cell.statics)
          collideBodies(cell.dynamics[a], s, tested);
      }
    }
  }
}

// An object in two cells at once would be tested twice: `tested` has the pairs
// that already were
void Stage::collideBodies(int a, int b, vector<long long> &tested) {
  long long key = ((long long)std::min(a, b) << 32) | (unsigned int)std::max(a, b);
  if (std::find(tested.begin(), tested.end(), key) != tested.end())
    return;
  tested.push_back(key);

  Body &A = bodies[a], &B = bodies[b];
  if (!A.object->isCollidable() || !B.object->isCollidable())
    return;
  Pose poseA = A.object->getPose(), poseB = B.object->getPose();
  vec3 minA, maxA, minB, maxB;
  A.object->getShape().bounds(poseA, minA, maxA);
  B.object->getShape().bounds(poseB, minB, maxB);
  if (maxA.x < minB.x || maxB.x < minA.x || maxA.y < minB.y ||
      maxB.y < minA.y || maxA.z < minB.z || maxB.z < minA.z)
    return; // not even their bounds touch

  Contact contact;
  shapeTests++;
  if (!CollisionShape::collide(A.object->getShape(), poseA,
                               B.object->getShape(), poseB, contact))
    return;

  // Who moves: all of it for a dynamic object against a static one, shared
  // (more for the lighter one) between two dynamic objects
  float invA = A.dynamic ? 1.0f / A.dynamic->getMass() : 0.0f;
  float invB = B.dynamic ? 1.0f / B.dynamic->getMass() : 0.0f;
  float sum = invA + invB;
  if (sum <= 0.0f)
    return;
  const vec3 &n = contact.normal;
  vec3 pushA = -n * (contact.depth * invA / sum);
  vec3 pushB = n * (contact.depth * invB / sum);

  // They stop approaching each other (no bounce)
  vec3 velocityA = A.dynamic ? A.dynamic->getVelocity() : vec3(0.0f);
  vec3 velocityB = B.dynamic ? B.dynamic->getVelocity() : vec3(0.0f);
  float approach = dot(velocityB - velocityA, n);
  vec3 changeA(0.0f), changeB(0.0f);
  if (approach < 0.0f) {
    float impulse = -approach / sum;
    changeA = -n * (impulse * invA);
    changeB = n * (impulse * invB);
  }
  if (A.dynamic)
    A.dynamic->applyCollision(pushA, changeA);
  if (B.dynamic)
    B.dynamic->applyCollision(pushB, changeB);
}

// The lowest points of the shape can't be under the floor
void Stage::collideShapeWithFloor(DynamicGameObject &object) const {
  if (!floor_mesh || !object.isCollidable())
    return;
  std::vector<vec3> samples;
  Pose pose = object.getPose();
  object.getShape().floorSamples(pose, samples);
  // Only a floor within the object's own height counts (not a ceiling above
  // it), however deep a point has sunk
  vec3 low, high;
  object.getShape().bounds(pose, low, high);
  float lift = 0.0f;
  for (const vec3 &s : samples) {
    float h;
    if (floorAt(s.x, s.z, h, nullptr, high.y))
      lift = std::max(lift, h - s.y);
  }
  if (lift <= 1e-4f)
    return;
  float falling = std::min(object.getVelocity().y, 0.0f);
  object.applyCollision(vec3(0.0f, lift, 0.0f), vec3(0.0f, -falling, 0.0f));
}
