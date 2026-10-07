#include "GameObject.h"
#include "RenderStats.h"

#include <cmath>

#include "TextFormat.h"
using namespace std;
using namespace glm;

// A person-sized pill, for what has no model to fit
static shared_ptr<const CollisionShape> personShape() {
  return make_shared<Capsule>(0.4f, 1.8f);
}

// A pill around the whole of a static model
static shared_ptr<const CollisionShape> fitShape(const Model &model) {
  vec3 min(1e30f), max(-1e30f);
  for (const Mesh &mesh : model.meshes)
    for (const Vertex &v : mesh.getVertices()) {
      min = glm::min(min, v.Position);
      max = glm::max(max, v.Position);
    }
  if (min.x > max.x)
    return personShape(); // no geometry
  return Capsule::fit(min, max);
}

GameObject::GameObject() : shape(personShape()) {}

GameObject::GameObject(shared_ptr<Model> model,
                       shared_ptr<const CollisionShape> shape)
    : shape(shape ? shape : fitShape(*model)) {
  addPart(model);
}

GameObject::GameObject(const char *modelPath,
                       shared_ptr<const CollisionShape> shape) {
  auto model = make_shared<Model>(modelPath);
  this->shape = shape ? shape : fitShape(*model);
  addPart(model);
}

GameObject::GameObject(shared_ptr<AnimatedModel> model,
                       shared_ptr<const CollisionShape> shape)
    : aniModel(model), shape(shape ? shape : personShape()) {}

size_t GameObject::addPart(shared_ptr<Model> model, int unlit) {
  Part part = {model, unlit, mat4(1.0f), true};
  parts.push_back(part);
  return parts.size() - 1;
}

void GameObject::addDetail(shared_ptr<Model> model, float distance) {
  if (details.empty() && !parts.empty())
    nearModel = parts[0].model;
  details.push_back({distance, model});
}

void GameObject::selectDetail(float distance) {
  if (details.empty() || parts.empty())
    return;
  shared_ptr<Model> chosen = nearModel;
  for (const Detail &d : details)
    if (distance >= d.from)
      chosen = d.model;
  parts[0].model = chosen;
}

void GameObject::setYaw(float radians) {
  rotation = glm::rotate(mat4(1.0f), radians, vec3(0.0f, 1.0f, 0.0f));
}

void GameObject::turn(float radians) {
  rotation = glm::rotate(mat4(1.0f), radians, vec3(0.0f, 1.0f, 0.0f)) * rotation;
}

float GameObject::getHeading() const {
  vec3 forward = vec3(rotation[2]);
  return std::atan2(forward.x, forward.z);
}

void GameObject::update(double dt) {
  time += dt;
  if (aniModel)
    aniModel->Update(time);
}

void GameObject::Draw(Shader *shader) {
  if (!visible)
    return;
  RenderStats::objects()++;
  shader->setFloat("breathAmp", breathAmp);
  shader->setFloat("swayAmp", swayAmp);

  if (aniModel) {
    mat4 transform = glm::scale(rotation, vec3(scale));
    shader->setVector3("objposition", position.x, position.y, position.z);
    shader->setMatrix4("objrotation", value_ptr(transform));
    aniModel->Draw(shader);
  }

  drawParts(shader, false);
  shader->setFloat("breathAmp", 0.0f);
  shader->setFloat("swayAmp", 0.0f);
}

void GameObject::DrawTransparent(Shader *shader) {
  if (!visible)
    return;
  shader->setFloat("breathAmp", breathAmp);
  shader->setFloat("swayAmp", swayAmp);
  drawParts(shader, true);
  shader->setFloat("breathAmp", 0.0f);
  shader->setFloat("swayAmp", 0.0f);
}

void GameObject::drawParts(Shader *shader, bool translucent) {
  shader->setInt("skinned", 0);
  for (const Part &part : parts) {
    if (!part.visible || (translucent && !part.model->hasTransparent()))
      continue;
    // world = position + rotation * (scale * local * p)
    mat4 placed = rotation * part.local;
    vec3 where = position + vec3(rotation * vec4(scale * vec3(part.local[3]), 0.0f));
    mat4 transform = glm::scale(placed, vec3(scale));
    transform[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    shader->setVector3("objposition", where.x, where.y, where.z);
    shader->setMatrix4("objrotation", value_ptr(transform));
    shader->setInt("unlit", part.unlit);
    part.model->Draw(shader, translucent);
  }
  shader->setInt("unlit", 0);
}

void GameObject::getProperties(vector<Property> &properties) {
  properties.push_back(Property::info("Posicion", [this]() { return textOf(position); }));
  properties.push_back(Property::toggle(
      "Visible", [this]() { return visible; }, [this](bool on) { visible = on; }));
}

void GameObject::describe(vector<string> &lines) const {
  lines.push_back("Posicion: " + textOf(position));
  // Heading: where its +z points, around +y (as setYaw); tilt: how far its
  // up axis leans from the vertical
  vec3 up = vec3(rotation[1]);
  float heading = degrees(getHeading());
  float tilt = degrees(std::acos(clamp(up.y / length(up), -1.0f, 1.0f)));
  lines.push_back(textFormat("Rumbo: %.1f grados  Inclinacion: %.1f grados",
                             tidy(heading), tilt));
  lines.push_back(textFormat("Escala: %.2f  Visible: %s  Colisionable: %s",
                             scale, visible ? "si" : "no",
                             collidable ? "si" : "no"));
  lines.push_back(aniModel ? "Modelo: animado (esqueleto)"
                           : textFormat("Modelo: %d pieza(s)", (int)parts.size()));

  Pose pose = getPose();
  if (const Capsule *capsule = dynamic_cast<const Capsule *>(shape.get())) {
    vec3 a, b;
    float r;
    capsule->segment(pose, a, b, r);
    lines.push_back(textFormat("Forma: capsula, radio %.2f, alto %.2f", r,
                               capsule->getHeight() * scale));
  } else if (const Box *box = dynamic_cast<const Box *>(shape.get())) {
    vec3 c, h;
    box->world(pose, c, h);
    lines.push_back(textFormat("Forma: caja %.2f x %.2f x %.2f", 2 * h.x,
                               2 * h.y, 2 * h.z));
    lines.push_back("  centro: " + textOf(c));
  }
  vec3 min, max;
  shape->bounds(pose, min, max);
  lines.push_back("AABB min: " + textOf(min));
  lines.push_back("AABB max: " + textOf(max));
  lines.push_back("AABB tamano: " + textOf(max - min));
}
