#include "House.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "NetBuffer.h"

using namespace glm;
using namespace std;

namespace {
// The measures are the Blender model's (assets/house/source/Casa_separada.obj); B() puts them in the
// house's frame as prepare_house.py does (SCALE, GROUND_Y, CENTRE_X, HINGE there: keep them in step)
const float SCALE = 0.42f, GROUND_Y = 0.69f, CENTRE_X = (-5.11f + 23.73f) / 2.0f;
const float HINGE_X = 5.13f, HINGE_Z = 1.35f;
const size_t HULL_DOOR = 0; // (the door's box: the first one)

vec3 B(float x, float y, float z) { return vec3((x - CENTRE_X) * SCALE, (y - GROUND_Y) * SCALE, z * SCALE); }
} // namespace

shared_ptr<CompoundShape> House::makeHull() {
  auto hull = make_shared<CompoundShape>();
  auto box = [&](float x0, float x1, float y0, float y1, float z0, float z1) {
    vec3 low = B(x0, y0, z0), high = B(x1, y1, z1);
    return hull->add((high - low) * 0.5f, (low + high) * 0.5f);
  };
  const float FLOOR = 3.35f, ROOF = 12.59f, IN = 5.04f, T = 0.3f, END = 19.36f;
  box(4.89f, 5.13f, 3.5f, 8.12f, -1.27f, 1.27f);          // the door, shut (HULL_DOOR)
  box(-5.11f, 5.11f, -6.48f, FLOOR, -19.39f, 19.39f);     // the block, its top the floor
  box(-5.11f, 5.11f, ROOF, ROOF + T, -19.39f, 19.39f);    // the ceiling
  box(-IN - T, -IN, FLOOR, ROOF, -19.39f, 19.39f);        // the -x wall
  box(-IN, IN, FLOOR, ROOF, -END - T, -END);              // the -z wall
  box(-IN, IN, FLOOR, ROOF, END, END + T);                // the +z wall
  box(IN, IN + 0.2f, FLOOR, ROOF, -19.39f, -1.27f);       // the +x wall, on each side of the doorway...
  box(IN, IN + 0.2f, FLOOR, ROOF, 1.27f, 19.39f);
  box(IN, IN + 0.2f, 8.12f, ROOF, -1.27f, 1.27f);         // ...and over it
  box(-3.6f, 2.6f, FLOOR, 5.78f, 15.64f, 19.3f);          // the bed
  box(5.14f, 23.73f, -1.5f, 1.67f, -19.39f, 19.39f);      // the porch's slab
  const float STEP_X[5] = {4.93f, 5.76f, 6.58f, 7.4f, 8.22f}, STEP_TOP[4] = {3.33f, 3.04f, 2.75f, 2.47f};
  for (int i = 0; i < 4; i++)
    box(STEP_X[i], STEP_X[i + 1], 1.67f, STEP_TOP[i], -1.44f, 1.44f); // the steps up to the door
  for (float x : {6.755f, 22.195f})
    for (float z : {17.51f, 5.815f, -5.88f, -17.575f})
      box(x - 0.2f, x + 0.2f, 1.67f, 10.2f, z - 0.2f, z + 0.2f); // the porch's beams
  return hull;
}

void House::footprint(vec3 &low, vec3 &high) {
  low = B(-5.11f, 0.0f, -19.39f) - vec3(1.0f, 0.0f, 1.0f);
  high = B(23.73f, 0.0f, 19.39f) + vec3(1.0f, 0.0f, 1.0f);
}

House::House(shared_ptr<Model> model, shared_ptr<Model> door) : House(model, door, makeHull()) {}

House::House(shared_ptr<Model> model, shared_ptr<Model> door, shared_ptr<CompoundShape> hull)
    : GameObject(model, hull), hull(hull) {
  doorPart = addPart(door);
  setDoorAngle(0.0f);
}

void House::setDoorAngle(float angle) {
  doorAngle = angle;
  // (it opens outwards: its free edge, along -z from the hinge, goes towards +x)
  setPartTransform(doorPart, glm::rotate(glm::translate(mat4(1.0f), B(HINGE_X, GROUND_Y, HINGE_Z)), -angle, vec3(0.0f, 1.0f, 0.0f)));
  hull->setEnabled(HULL_DOOR, angle < 0.05f);
}

vec3 House::doorPoint() const {
  vec3 local = B(5.6f, 3.33f, 0.0f);
  return position + vec3(getRotationMatrix() * vec4(local * scale, 0.0f));
}

HouseDoor::HouseDoor(shared_ptr<Model> model, shared_ptr<House> house) : DynamicGameObject(model), house(house) {
  setGravity(0.0f);
  setCollidable(false); // (the house's own box is the door)
  setVisible(false);    // (the house draws it)
  position = house->doorPoint();
}

void HouseDoor::update(double dt) {
  GameObject::update(dt);
  velocity = vec3(0.0f);
  float goal = open ? House::DOOR_OPEN_ANGLE : 0.0f, step = SPEED * (float)dt;
  angle += std::max(-step, std::min(step, goal - angle));
  house->setDoorAngle(angle);
}

// u8 open, f32 angle
void HouseDoor::writeNetState(NetWriter &out) const {
  out.u8(open ? 1 : 0);
  out.f32(angle);
}

void HouseDoor::readNetState(NetReader &in) {
  uint8_t o = in.u8();
  float a = in.f32();
  if (!in.isOk())
    return;
  open = o != 0;
  if (std::fabs(a - angle) > 0.5f) // (it swings here as on the server; only a big gap snaps it)
    angle = a;
}
