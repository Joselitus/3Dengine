#include "DynamicGameObject.h"

#include <cmath>

#include "TextFormat.h"
using namespace glm;

void DynamicGameObject::steerTowards(const vec3 &wantedVelocity,
                                     float responsiveness) {
  vec3 a = (wantedVelocity - velocity) * responsiveness;
  float len = length(a);
  if (len > maxAcceleration)
    a *= maxAcceleration / len;
  acceleration = a;
}

void DynamicGameObject::update(double dt) {
  GameObject::update(dt);
  if (replica)
    return; // (it is where the server says, not where its speed takes it)
  velocity += (acceleration - vec3(0.0f, gravity, 0.0f)) * (float)dt;
  // maxSpeed limits the horizontal speed only, so falling is not capped
  float speed = length(vec2(velocity.x, velocity.z));
  if (speed > maxSpeed) {
    velocity.x *= maxSpeed / speed;
    velocity.z *= maxSpeed / speed;
  }
  // Drag on the ground plane (not on falling): exponential, so it does not
  // depend on the frame rate
  if (drag > 0.0f) {
    float keep = std::exp(-drag * (float)dt);
    velocity.x *= keep;
    velocity.z *= keep;
  }
  position += velocity * (float)dt;
}

void DynamicGameObject::registerFirstMesh() {
  if (!meshes.empty())
    return;
  MeshOption first;
  first.animated = aniModel;
  if (!aniModel && !parts.empty()) {
    first.model = parts[0].model;
    hasMainPart = true;
    mainPart = 0;
  }
  meshes.push_back(first);
  currentMesh = 0;
}

size_t DynamicGameObject::addMesh(std::shared_ptr<AnimatedModel> model) {
  registerFirstMesh();
  MeshOption option;
  option.animated = model;
  meshes.push_back(option);
  return meshes.size() - 1;
}

size_t DynamicGameObject::addMesh(std::shared_ptr<Model> model) {
  registerFirstMesh();
  MeshOption option;
  option.model = model;
  meshes.push_back(option);
  return meshes.size() - 1;
}

void DynamicGameObject::setMesh(size_t index) {
  if (index >= meshes.size())
    return; // (with no meshes added there is only mesh 0, already shown)
  currentMesh = index;
  const MeshOption &mesh = meshes[index];
  aniModel = mesh.animated; // null for a static one: update() leaves the others alone
  if (mesh.model) {
    if (hasMainPart) {
      parts[mainPart].model = mesh.model;
      parts[mainPart].visible = true;
    } else {
      mainPart = addPart(mesh.model);
      hasMainPart = true;
    }
  } else if (hasMainPart) {
    parts[mainPart].visible = false; // an animated mesh is shown instead
  }
}

void DynamicGameObject::getProperties(std::vector<Property> &properties) {
  GameObject::getProperties(properties);
  properties.push_back(Property::number(
      "Velocidad", 0.0f, 30.0f, 0.0f,
      [this]() { return length(vec3(velocity.x, 0.0f, velocity.z)); },
      [this](float speed) {
        vec3 horizontal(velocity.x, 0.0f, velocity.z);
        vec3 direction = length(horizontal) > 1e-3f ? normalize(horizontal)
                                                    : vec3(rotation[2]);
        direction.y = 0.0f;
        if (length(direction) < 1e-3f)
          return;
        direction = normalize(direction);
        velocity = vec3(direction.x * speed, velocity.y, direction.z * speed);
      },
      "m/s"));
  properties.push_back(Property::number(
      "Velocidad maxima", 0.0f, 30.0f, 0.5f, [this]() { return maxSpeed; },
      [this](float v) { maxSpeed = v; }, "m/s"));
  properties.push_back(Property::number(
      "Rozamiento", 0.0f, 20.0f, 0.5f, [this]() { return drag; },
      [this](float d) { drag = d; }, "/s"));
  properties.push_back(Property::number(
      "Gravedad", 0.0f, 30.0f, 0.1f, [this]() { return gravity; },
      [this](float g) { gravity = g; }, "m/s2"));
}

void DynamicGameObject::describe(std::vector<std::string> &lines) const {
  GameObject::describe(lines);
  lines.push_back(textFormat("Velocidad: %s  %.2f m/s",
                             textOf(velocity).c_str(), length(velocity)));
  lines.push_back("Aceleracion: " + textOf(acceleration));
  lines.push_back(textFormat("Masa: %.0f kg  Gravedad: %.1f m/s2", getMass(),
                             gravity));
  lines.push_back(textFormat("Vel. max: %.1f m/s  Rozamiento: %.1f /s", maxSpeed,
                             drag));
  lines.push_back(std::string("En el suelo: ") + (grounded ? "si" : "no"));
  if (getMeshCount() > 1)
    lines.push_back(textFormat("Malla: %d de %d", (int)currentMesh + 1,
                               (int)getMeshCount()));
}
