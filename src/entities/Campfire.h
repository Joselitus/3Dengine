#ifndef CAMPFIRE
#define CAMPFIRE

#include <memory>
#include <vector>

#include "GameObject.h"
#include "ParticleEmitter.h"
#include "SafeSpace.h"
#include "SpotLight.h"

// A campfire (assets/campfire, generate_campfire.py): a ring of stones with logs burning in it and
// two thick logs to sit on beside it. It burns all the time: three flames that flicker (parts drawn
// glowing), glowing embers, an orange light that flickers (getLight), smoke and sparks (getEmitters:
// the map adds them to its own).
//
// The enemies fear the fire: none of them comes within FEAR_RADIUS of it (the map drives them out:
// VehicleStage::keepFromFire), and a player within that distance is in a safe place (shelter: a
// SafeSpace), so they behave as when he is in the RV. A static object (the map editor's prop
// «Hoguera»: GameStage::propPlaced does the rest); solid where it burns and where the logs lie.
class Campfire : public GameObject {
  size_t embersPart = 0;
  size_t flameParts[3] = {0, 0, 0};
  std::vector<std::shared_ptr<ParticleEmitter>> emitters; // smoke, sparks
  bool emittersPlaced = false;
  void placeEmitters();

public:
  static constexpr float FIRE_RADIUS = 0.55f; // the ring of stones (generate_campfire.py)
  static constexpr float FEAR_RADIUS = 10.0f; // m: the enemies keep out of this, and it is a safe place
  static constexpr float LIGHT_RANGE = 14.0f;  // m

  // `model`: campfire.obj; `embers`: campfire_embers.obj; `flame`: campfire_flame.obj
  Campfire(std::shared_ptr<Model> model, std::shared_ptr<Model> embers, std::shared_ptr<Model> flame);

  // The fire's middle, on the ground (world)
  glm::vec3 fireCentre() const { return position; }
  bool scares(const glm::vec3 &point) const;
  // The safe place round `fire` (to add to the stage: Stage::addSafeSpace)
  static std::shared_ptr<SafeSpace> shelter(std::shared_ptr<const Campfire> fire);
  void update(double dt) override;
  void getLight(std::vector<SpotLight> &lights) const;
  const std::vector<std::shared_ptr<ParticleEmitter>> &getEmitters() const { return emitters; }
};

#endif
