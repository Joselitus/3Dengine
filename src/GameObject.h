#ifndef GAME_OBJECT
#define GAME_OBJECT

#include <glm/ext/matrix_float4x4.hpp>
#include <vector>

#include <glm/glm.hpp>

#include "AnimatedMesh.h"
#include "AnimatedModel.h"
#include "Model.h"

class GameObject {
private:
  bool anim;
  Model *model;
  AnimatedModel *aniModel;
  glm::vec3 position = glm::vec3(0.0f);
  glm::mat4 rotation = glm::mat4(1.0f);

public:
  GameObject(Model *model);
  GameObject(AnimatedModel *model);
  GameObject(Model *model, float x, float y, float z);
  void setPosition(float x, float y, float z) {
    this->position = glm::vec3(x, y, z);
  }
  void setRotation(glm::mat4 rotation) { this->rotation = rotation; }
  glm::vec3 getPosition() const { return position; }
  void translate(const glm::vec3 &delta) { this->position += delta; }
  void Draw(Shader *shader);
};

#endif
