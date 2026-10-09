#ifndef DEATH_POSE
#define DEATH_POSE

#include <memory>

#include <glm/glm.hpp>

#include "AnimatedModel.h"

// The player's penguin (PenguinoAnimado.fbx) dying, posed from code every frame (like SeatedPose). It
// does what the death camera shows: it stands stiff like a rod on its feet and tips over backwards,
// slowly at first and then faster (theta'' = 3g/2L sin(theta), L = EYE_HEIGHT, the same numbers as the
// client's camera: fallAngle() is what the camera uses), flailing its flippers as it goes, and ends
// lying on its back on the floor in a comic pose: flippers thrown wide, legs splayed and kicked up,
// the head dropped back (a gaping beak). Frame: the object's, in metres, feet at the origin, facing +z,
// its left on +x. Pure maths, no OpenGL; whoever owns it steps it and then applies it.
class DeathPose {
public:
  explicit DeathPose(std::shared_ptr<AnimatedModel> model);

  // The height of the eyes the camera falls from (the rod's length)
  static constexpr float EYE_HEIGHT = 1.6f;
  static constexpr float HALF_TURN = 1.5707963f;

  bool isValid() const { return valid; }
  // Advances the fall
  void step(double dt);
  // Poses the model
  void apply();
  // How far over it is (0 upright .. HALF_TURN lying on its back)
  float fallAngle() const { return angle; }
  // Starts over (upright)
  void restart();

private:
  std::shared_ptr<AnimatedModel> model;
  bool valid = false;
  double time = 0.0;
  float angle = 0.0f, speed = 0.0f;
  double landed = -1.0; // when it first touched the floor (-1: not yet)

  glm::mat4 toModelUnits(const glm::mat4 &metres) const;
};

#endif
