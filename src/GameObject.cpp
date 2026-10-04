#include "GameObject.h"
using namespace std;
using namespace glm;

GameObject::GameObject(shared_ptr<Model> model) { addPart(model); }

GameObject::GameObject(const char *modelPath) {
  addPart(make_shared<Model>(modelPath));
}

GameObject::GameObject(shared_ptr<AnimatedModel> model) : aniModel(model) {}

void GameObject::addPart(shared_ptr<Model> model, int unlit) {
  Part part = {model, unlit};
  parts.push_back(part);
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
  shader->setVector3("objposition", position.x, position.y, position.z);
  mat4 transform = glm::scale(rotation, vec3(scale));
  shader->setMatrix4("objrotation", value_ptr(transform));
  shader->setFloat("breathAmp", breathAmp);

  if (aniModel)
    aniModel->Draw(shader);

  shader->setInt("skinned", 0);
  for (const Part &part : parts) {
    shader->setInt("unlit", part.unlit);
    part.model->Draw(shader);
  }
  shader->setInt("unlit", 0);
  shader->setFloat("breathAmp", 0.0f);
}
