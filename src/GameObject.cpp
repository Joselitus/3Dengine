#include "GameObject.h"
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
  Part part = {model, unlit, mat4(1.0f)};
  parts.push_back(part);
  return parts.size() - 1;
}

void GameObject::setYaw(float radians) {
  rotation = glm::rotate(mat4(1.0f), radians, vec3(0.0f, 1.0f, 0.0f));
}

void GameObject::update(double dt) {
  time += dt;
  if (aniModel)
    aniModel->Update(time);
}

void GameObject::Draw(Shader *shader) {
  if (!visible)
    return;
  shader->setFloat("breathAmp", breathAmp);

  if (aniModel) {
    mat4 transform = glm::scale(rotation, vec3(scale));
    shader->setVector3("objposition", position.x, position.y, position.z);
    shader->setMatrix4("objrotation", value_ptr(transform));
    aniModel->Draw(shader);
  }

  shader->setInt("skinned", 0);
  for (const Part &part : parts) {
    // world = position + rotation * (scale * local * p)
    mat4 placed = rotation * part.local;
    vec3 where = position + vec3(rotation * vec4(scale * vec3(part.local[3]), 0.0f));
    mat4 transform = glm::scale(placed, vec3(scale));
    transform[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    shader->setVector3("objposition", where.x, where.y, where.z);
    shader->setMatrix4("objrotation", value_ptr(transform));
    shader->setInt("unlit", part.unlit);
    part.model->Draw(shader);
  }
  shader->setInt("unlit", 0);
  shader->setFloat("breathAmp", 0.0f);
}
