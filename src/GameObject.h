#ifndef GAME_OBJECT
#define GAME_OBJECT

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "AnimatedModel.h"
#include "Model.h"

// Anything that is placed in the world. It owns (shares) the meshes it is
// made of and is in charge of rendering them.
class GameObject {
public:
  // One model of the object and how the shader must draw it
  struct Part {
    std::shared_ptr<Model> model;
    int unlit; // 0 = lit, 2 = emissive (see shader.frag)
  };

protected:
  std::vector<Part> parts;
  std::shared_ptr<AnimatedModel> aniModel; // skinned model, if it has one
  glm::vec3 position = glm::vec3(0.0f);
  glm::mat4 rotation = glm::mat4(1.0f);
  float scale = 1.0f;
  float breathAmp = 0.0f; // procedural breathing of the static meshes
  double time = 0.0;      // seconds since the object was updated the first time

public:
  GameObject() {}
  GameObject(std::shared_ptr<Model> model);
  GameObject(const char *modelPath);
  GameObject(std::shared_ptr<AnimatedModel> model);
  virtual ~GameObject() {}

  void addPart(std::shared_ptr<Model> model, int unlit = 0);

  void setPosition(float x, float y, float z) { position = glm::vec3(x, y, z); }
  void setRotation(const glm::mat4 &rotation) { this->rotation = rotation; }
  void setYaw(float radians);
  void setScale(float scale) { this->scale = scale; }
  void setBreathAmp(float amp) { breathAmp = amp; }
  glm::vec3 getPosition() const { return position; }
  void translate(const glm::vec3 &delta) { position += delta; }

  // Advances the object by dt seconds
  virtual void update(double dt);
  void Draw(Shader *shader);
};

#endif
