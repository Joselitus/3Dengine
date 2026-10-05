#ifndef DYNAMIC_GAME_OBJECT
#define DYNAMIC_GAME_OBJECT

#include "GameObject.h"

class Stage;

// A GameObject that moves by itself: characters, enemies... The controller
// (or any AI) steers it through its acceleration; update() integrates it.
//
// It can also change the mesh it is drawn with: the model it was made with is
// mesh 0 and addMesh() gives it more (static or animated), all loaded from the
// start so that setMesh() just swaps which one is drawn. Only the mesh that is
// shown is updated, so an animation that is not on screen does not run.
class DynamicGameObject : public GameObject {
private:
  // One mesh the object can be drawn with: a static model or an animated one
  struct MeshOption {
    std::shared_ptr<Model> model;
    std::shared_ptr<AnimatedModel> animated;
  };
  std::vector<MeshOption> meshes; // empty until the second mesh is added
  size_t currentMesh = 0;
  bool hasMainPart = false; // the shown static model is parts[mainPart]
  size_t mainPart = 0;

  void registerFirstMesh(); // the model it was made with becomes mesh 0

protected:
  glm::vec3 velocity = glm::vec3(0.0f);     // units / second
  glm::vec3 acceleration = glm::vec3(0.0f); // units / second^2
  float maxSpeed = 4.0f;                    // units / second
  float maxAcceleration = 40.0f;            // units / second^2
  float gravity = 0.0f;   // units / second^2 pulling down (0 = flies)
  bool grounded = false;  // standing on the floor, set by the stage
  float mass = 70.0f;     // kg: the lighter one is pushed more in a collision
  float drag = 0.0f;      // 1/s: how fast the horizontal velocity dies out
                          // (0 = never; big = stops soon after a push)

public:
  using GameObject::GameObject;

  // Adds a mesh the object can show; returns its index (the model it was
  // made with is 0). It is not shown until setMesh().
  size_t addMesh(std::shared_ptr<AnimatedModel> model);
  size_t addMesh(std::shared_ptr<Model> model);
  // Shows mesh `index` instead of the current one (an unknown index is
  // ignored). The object's other parts (wheels...) are not touched.
  void setMesh(size_t index);
  size_t getMesh() const { return currentMesh; }
  size_t getMeshCount() const { return meshes.empty() ? 1 : meshes.size(); }

  void setVelocity(const glm::vec3 &v) { velocity = v; }
  void setAcceleration(const glm::vec3 &a) { acceleration = a; }
  void setMaxSpeed(float speed) { maxSpeed = speed; }
  void setMaxAcceleration(float accel) { maxAcceleration = accel; }
  void setGravity(float g) { gravity = g; }
  void setMass(float m) { mass = m; }
  void setDrag(float d) { drag = d; }
  void setGrounded(bool g) { grounded = g; }
  glm::vec3 getVelocity() const { return velocity; }
  glm::vec3 getAcceleration() const { return acceleration; }
  float getMaxSpeed() const { return maxSpeed; }
  float getGravity() const { return gravity; }
  bool isGrounded() const { return grounded; }
  virtual float getMass() const { return mass; }
  float getDrag() const { return drag; }
  float getMaxAcceleration() const { return maxAcceleration; }

  // Accelerates towards a wanted velocity, never faster than maxAcceleration
  void steerTowards(const glm::vec3 &wantedVelocity, float responsiveness);

  void update(double dt) override;
  // It also stops: no velocity or acceleration
  void teleport(const glm::vec3 &position) override {
    GameObject::teleport(position);
    velocity = acceleration = glm::vec3(0.0f);
  }
  // Adds the motion: velocity, acceleration, mass, gravity...
  void describe(std::vector<std::string> &lines) const override;
  // Adds its speed (along where it is going, or where it faces if it is
  // still), top speed, drag and gravity
  void getProperties(std::vector<Property> &properties) override;

  // Called by the stage instead of its default floor handling (snapping the
  // object on the floor). Return true if the object deals with the floor
  // itself (e.g. a vehicle on its suspension), moving itself dt seconds.
  virtual bool contactFloor(const Stage &stage, double dt) { return false; }

  // The stage calls this when the object's collision shape has hit something:
  // it has to be moved by `push` and its velocity changed by `velocityChange`.
  // Objects that keep their own state (like a vehicle) override it.
  virtual void applyCollision(const glm::vec3 &push,
                              const glm::vec3 &velocityChange) {
    position += push;
    velocity += velocityChange;
  }
};

#endif
