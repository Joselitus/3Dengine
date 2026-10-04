#ifndef DYNAMIC_GAME_OBJECT
#define DYNAMIC_GAME_OBJECT

#include "GameObject.h"

// A GameObject that moves by itself: characters, enemies... The controller
// (or any AI) steers it through its acceleration; update() integrates it.
class DynamicGameObject : public GameObject {
protected:
  glm::vec3 velocity = glm::vec3(0.0f);     // units / second
  glm::vec3 acceleration = glm::vec3(0.0f); // units / second^2
  float maxSpeed = 4.0f;                    // units / second
  float maxAcceleration = 40.0f;            // units / second^2
  float gravity = 0.0f;   // units / second^2 pulling down (0 = flies)
  bool grounded = false;  // standing on the floor, set by the stage

public:
  using GameObject::GameObject;

  void setVelocity(const glm::vec3 &v) { velocity = v; }
  void setAcceleration(const glm::vec3 &a) { acceleration = a; }
  void setMaxSpeed(float speed) { maxSpeed = speed; }
  void setMaxAcceleration(float accel) { maxAcceleration = accel; }
  void setGravity(float g) { gravity = g; }
  void setGrounded(bool g) { grounded = g; }
  glm::vec3 getVelocity() const { return velocity; }
  glm::vec3 getAcceleration() const { return acceleration; }
  float getMaxSpeed() const { return maxSpeed; }
  float getGravity() const { return gravity; }
  bool isGrounded() const { return grounded; }
  float getMaxAcceleration() const { return maxAcceleration; }

  // Accelerates towards a wanted velocity, never faster than maxAcceleration
  void steerTowards(const glm::vec3 &wantedVelocity, float responsiveness);

  void update(double dt) override;
};

#endif
