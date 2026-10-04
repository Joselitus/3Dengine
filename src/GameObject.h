#ifndef GAME_OBJECT
#define GAME_OBJECT

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "AnimatedModel.h"
#include "CollisionShape.h"
#include "Model.h"

// Anything that is placed in the world. It shares the models (parts) it is
// made of, or an AnimatedModel, plus a position, a rotation and a scale, and
// is in charge of rendering them: Draw() sets objposition/objrotation (which
// includes the scale) and the unlit/breathAmp uniforms, then draws the parts.
// It also has a collision shape (see CollisionShape): by default a pill
// (Capsule) that fits its model; another shape can be given on construction.
class GameObject {
public:
  // One model of the object and how the shader must draw it
  struct Part {
    std::shared_ptr<Model> model;
    int unlit; // 0 = lit, 2 = emissive (see shader.frag)
    // Placement of the part relative to the object (identity: where the
    // object is); e.g. a wheel that moves with the suspension
    glm::mat4 local;
  };

protected:
  std::vector<Part> parts;
  std::shared_ptr<AnimatedModel> aniModel; // skinned model, if it has one
  glm::vec3 position = glm::vec3(0.0f);
  glm::mat4 rotation = glm::mat4(1.0f);
  float scale = 1.0f;
  float breathAmp = 0.0f; // procedural breathing of the static meshes
  bool visible = true;    // false: Draw() does nothing (e.g. a first-person
                          // player, whose model would hide the view)
  double time = 0.0;      // seconds since the object was updated the first time
  std::shared_ptr<const CollisionShape> shape; // never null
  bool collidable = true; // false: the stage ignores its shape (e.g. the floor)

public:
  // `shape` is the collision shape, in the object's frame; without it, a
  // capsule fitted to the model (to a 1.8 tall pill for an animated model).
  GameObject();
  GameObject(std::shared_ptr<Model> model,
             std::shared_ptr<const CollisionShape> shape = nullptr);
  GameObject(const char *modelPath,
             std::shared_ptr<const CollisionShape> shape = nullptr);
  GameObject(std::shared_ptr<AnimatedModel> model,
             std::shared_ptr<const CollisionShape> shape = nullptr);
  virtual ~GameObject() {}

  // Returns the index of the part
  size_t addPart(std::shared_ptr<Model> model, int unlit = 0);
  void setPartTransform(size_t part, const glm::mat4 &local) {
    parts[part].local = local;
  }

  void setPosition(float x, float y, float z) { position = glm::vec3(x, y, z); }
  void setRotation(const glm::mat4 &rotation) { this->rotation = rotation; }
  void setYaw(float radians);
  void setScale(float scale) { this->scale = scale; }
  void setBreathAmp(float amp) { breathAmp = amp; }
  void setVisible(bool visible) { this->visible = visible; }
  bool isVisible() const { return visible; }
  glm::vec3 getPosition() const { return position; }
  const CollisionShape &getShape() const { return *shape; }
  void setCollidable(bool collidable) { this->collidable = collidable; }
  bool isCollidable() const { return collidable; }
  // Where the collision shape is now
  Pose getPose() const {
    Pose pose;
    pose.position = position;
    pose.rotation = glm::mat3(rotation);
    pose.scale = scale;
    return pose;
  }
  void translate(const glm::vec3 &delta) { position += delta; }

  // Advances the object by dt seconds
  virtual void update(double dt);
  void Draw(Shader *shader);
};

#endif
