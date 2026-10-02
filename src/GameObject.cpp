#include "GameObject.h"
using namespace std;
using namespace glm;

GameObject::GameObject(Model *model) {
  this->model = model;
  this->aniModel = nullptr;
  this->anim = false;
}

GameObject::GameObject(AnimatedModel *model) {

  this->aniModel = model;
  this->model = nullptr;
  this->anim = true;
}

GameObject::GameObject(Model *model, float x, float y, float z)
    : GameObject(model) {
  this->position = vec3(x, y, z);
}

void GameObject::Draw(Shader *shader) {
  shader->setVector3("objposition", position.x, position.y, position.z);
  shader->setMatrix4("objrotation", value_ptr(this->rotation));
  if (!this->anim) {
    shader->setInt("skinned", 0);
    this->model->Draw(shader);
  }
  else
    this->aniModel->Draw(shader);
}
