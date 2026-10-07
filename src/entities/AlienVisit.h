#ifndef ALIEN_VISIT
#define ALIEN_VISIT

#include <functional>
#include <memory>

#include <glm/glm.hpp>

#include "Bob.h"
#include "Saucer.h"

class Stage;

// Bob's night visits to a map: his ship (Saucer) and Bob, made from assets/bob, added to `stage`
// and wired to each other and to the map. The ship lands on `landing` (the ground under its
// middle) with its ramp opening towards `rampYaw` (radians about +y, 0 = +z). The map says
// whether it is night, where the player is (his feet), whether he is in the vehicle and whether
// he is dead, and what happens when Bob takes him (`abduct`, given the ship's hatch: the map
// calls GameStage::abductPlayer), when his ray hits him (`paralyse`, for that many seconds; and
// `paralysed`: whether he still is) and when the player uses the ship's ramp (`enterShip`). The
// map must also add the ship's and Bob's lights to its spot lights (Saucer::getLights,
// Bob::getLight), and register the ship as an Interactable.
struct AlienVisit {
  std::shared_ptr<Saucer> saucer;
  std::shared_ptr<Bob> bob;

  static AlienVisit create(Stage &stage, const glm::vec3 &landing, float rampYaw,
                           std::function<bool()> night, std::function<glm::vec3()> player,
                           std::function<bool()> inVehicle, std::function<bool()> playerDead,
                           std::function<void(const glm::vec3 &)> abduct,
                           std::function<void(float)> paralyse, std::function<bool()> paralysed,
                           std::function<void()> enterShip);
};

#endif
