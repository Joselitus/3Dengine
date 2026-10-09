#ifndef SEATED_POSE
#define SEATED_POSE

#include <memory>
#include <random>

#include <glm/glm.hpp>

#include "AnimatedModel.h"

// The player's penguin (PenguinoAnimado.fbx) sitting in a seat of the RV, posed from code every frame
// (AnimatedModel::setBoneGlobals). The model's mesh is a penguin in a T pose whose lower half is
// weighted to the leg bones: sitting, that half goes forward over the cushion like thighs, the
// bottom of it hangs from the knees and the toes point up. Its flippers are what moves:
//  - the driver leans forward and holds the steering wheel's rim with his left flipper (it follows
//    the rim as the wheel turns), and has a can of beer in his right one: he rests it on his lap and
//    every 8 to 16 s lifts it to his beak and drinks;
//  - the passenger leans back with both flippers resting on his lap.
// Everything here is in the object's frame, in metres: the penguin's feet at the origin, facing +z,
// his left on +x (the walker's frame: the stage turns the walker with the RV). Pure maths, no
// OpenGL; whoever owns it steps it and then applies it.
class SeatedPose {
public:
  explicit SeatedPose(std::shared_ptr<AnimatedModel> model);

  // Where the object (its origin) goes, from the point of the seat's cushion where he sits (under his
  // hips); same axes
  static glm::vec3 originFromSeat();

  // False if the model lacks the bones this needs
  bool isValid() const { return valid; }
  // Driving (wheel grip and beer) or riding as the passenger
  void setDriver(bool driver) { this->driver = driver; }
  // Where the driver's left flipper holds the steering wheel's rim (object frame)
  void setGrip(const glm::vec3 &point) { grip = point; }
  // Advances the breathing and the sips of beer
  void step(double dt);
  // Poses the model
  void apply();
  // Where the can is, in the object's frame (the model's local transform of a part): its centre in
  // the right flipper, its axis (+y) upright or tipped to the beak
  glm::mat4 canTransform() const;

private:
  std::shared_ptr<AnimatedModel> model;
  bool valid = false;
  bool driver = true;
  glm::vec3 grip = glm::vec3(0.0f, 0.0f, 1.0f);
  double time = 0.0;
  double nextSip = 0.0, sipStart = -100.0; // when he lifts the can next / lifted it last
  std::mt19937 random;

  float lean() const;
  float sipAmount() const; // 0 resting .. 1 the can at the beak
  // The whole body (but the flippers and toes) leans about the point it sits on
  glm::mat4 bodyTransform() const;
  // A flipper's root and direction once posed (side 0 = left, 1 = right)
  glm::vec3 flipperRoot(int side) const;
  glm::vec3 flipperDirection(int side) const;
  // From a transform of the object's frame in metres to one in the model's own units
  glm::mat4 toModelUnits(const glm::mat4 &metres) const;
};

#endif
