#ifndef GAME_OBJECT
#define GAME_OBJECT

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "AnimatedModel.h"
#include "CollisionShape.h"
#include "Model.h"
#include "Property.h"

class Stage;

// Anything that is placed in the world. It shares the models (parts) it is
// made of, or an AnimatedModel, plus a position, a rotation and a scale, and
// is in charge of rendering them: Draw() sets objposition/objrotation (which
// includes the scale) and the unlit/breathAmp uniforms, then draws the parts.
// It also has a collision shape (see CollisionShape): by default a pill
// (Capsule) that fits its model; another shape can be given on construction.
// describe() lists its state as text, and getProperties() the values that
// can be changed from the debug inspector (see DebugSelector).
class GameObject {
public:
  // One model of the object and how the shader must draw it
  struct Part {
    std::shared_ptr<Model> model;
    int unlit; // 0 = lit, 1/3 = sky, 2 = emissive (see shader.frag)
    // Placement of the part relative to the object (identity: where the
    // object is); e.g. a wheel that moves with the suspension
    glm::mat4 local;
    bool visible; // false: not drawn (e.g. the lit lenses of lamps)
  };

protected:
  std::vector<Part> parts;
  std::shared_ptr<Model> nearModel; // the first part's own model, with details
  std::shared_ptr<AnimatedModel> aniModel; // skinned model, if it has one
  glm::vec3 position = glm::vec3(0.0f);
  glm::mat4 rotation = glm::mat4(1.0f);
  float scale = 1.0f;
  float breathAmp = 0.0f; // procedural breathing of the static meshes
  float swayAmp = 0.0f;   // wind on a tree: 0 = still, 1 = a normal breeze (see setSwayAmp)
  bool visible = true;    // false: Draw() does nothing (e.g. a first-person
                          // player, whose model would hide the view)
  double time = 0.0;      // seconds since the object was updated the first time
  std::shared_ptr<const CollisionShape> shape; // never null
  bool collidable = true; // false: the stage ignores its shape (e.g. the floor)
  float cullRadius = 0.0f; // how far its parts reach from its position (see setCullRadius)
  // Simpler models of the first part, for when the object is far from the camera
  struct Detail {
    float from; // metres from the camera (in x/z) from which it is used
    std::shared_ptr<Model> model;
  };
  std::vector<Detail> details; // by increasing distance; the part's own model is used before the first

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
  void setPartVisible(size_t part, bool visible) {
    parts[part].visible = visible;
  }
  // How the shader draws the part: 0 lit, 2 emissive (flat colour, it glows)
  void setPartUnlit(size_t part, int unlit) { parts[part].unlit = unlit; }

  void setPosition(float x, float y, float z) { position = glm::vec3(x, y, z); }
  void setRotation(const glm::mat4 &rotation) { this->rotation = rotation; }
  void setYaw(float radians);
  void setScale(float scale) { this->scale = scale; }
  void setBreathAmp(float amp) { breathAmp = amp; }
  // The vertex shader moves the object like a tree in the wind (the mesh must stand on
  // y = 0 with its trunk on the y axis); 0 = still
  void setSwayAmp(float amp) { swayAmp = amp; }
  void setVisible(bool visible) { this->visible = visible; }
  bool isVisible() const { return visible; }
  glm::vec3 getPosition() const { return position; }
  const CollisionShape &getShape() const { return *shape; }
  void setCollidable(bool collidable) { this->collidable = collidable; }
  // For Stage::setDrawDistance: how far from its position the object extends (a
  // big object, like a chunk of forest or the terrain, is drawn while any of it
  // can be in range)
  void setCullRadius(float radius) { cullRadius = radius; }
  float getCullRadius() const { return cullRadius; }
  // From `distance` metres away the first part is drawn with `model`, a simpler version of
  // it (call it for the nearest first); the stage chooses with selectDetail before drawing
  void addDetail(std::shared_ptr<Model> model, float distance);
  bool hasDetails() const { return !details.empty(); }
  // Uses the version of the model that suits an object `distance` metres from the camera
  void selectDetail(float distance);
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
  // Puts the object somewhere else at once, as if it had always been there
  // (e.g. the debug placement mode). Objects with more state than their
  // position (velocity, a physics body, other parts) override it. To move an
  // object of a stage, use Stage::relocate, which also updates its grid.
  virtual void teleport(const glm::vec3 &position) {
    this->position = position;
  }
  // Turns the object `radians` around the world's vertical axis, through its
  // position (positive: anticlockwise seen from above, like setYaw). Its tilt
  // and any scale in the rotation are kept. As with teleport, objects of a
  // stage are turned through it (Stage::turn).
  virtual void turn(float radians);
  // Which way it faces around the vertical, radians (as setYaw: 0 = its +z
  // towards the world's +z); turn(r) adds r to it
  virtual float getHeading() const;

  // Advances the object by dt seconds
  virtual void update(double dt);
  // Appends what there is to know about the object's state, one line each
  // (position, rotation, shape and bounds...), for the debug selector.
  // Subclasses add their own lines after their parent's.
  virtual void describe(std::vector<std::string> &lines) const;
  // Appends its values that the debug inspector shows and can change (see
  // Property). The functions are bound to this object: they must not outlive
  // it. Subclasses add theirs after their parent's.
  virtual void getProperties(std::vector<Property> &properties);
  // A shot (Bob's ship's ray gun) hits it, going along `direction`, in `stage`: it loses `amount`
  // of its health (1 = all of it). Nothing happens by default; the creatures override it.
  virtual void takeDamage(float amount, const glm::vec3 &direction, const Stage &stage) {}
  void Draw(Shader *shader); // the opaque meshes
  // The translucent meshes of its parts (windows...): the stage draws them
  // after every object's opaque ones
  void DrawTransparent(Shader *shader);

private:
  void drawParts(Shader *shader, bool translucent);
};

#endif
