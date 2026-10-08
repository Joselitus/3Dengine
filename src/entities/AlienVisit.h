#ifndef ALIEN_VISIT
#define ALIEN_VISIT

#include <functional>
#include <memory>

#include <glm/glm.hpp>

#include "Bob.h"
#include "Saucer.h"

class Stage;

// Bob's night visits to a map: his ship (Saucer) and Bob, made from assets/bob, added to `stage`
// (with the ship's emitters: smoke and sparks)
// and wired to each other and to the map. The ship lands on `landing` (the ground under its
// middle) with its ramp opening towards `rampYaw` (radians about +y, 0 = +z). The map says
// whether it is night, who is about for Bob to go for (`findVictim`, see Bob::setVictimQuery),
// and what happens when Bob takes a player (`abduct`, given his id and the ship's hatch: the map
// calls GameStage::abductPlayer), when his ray hits one (`paralyse`: his id and for how many
// seconds) and when a player uses the ship's ramp (`enterShip`). The
// map must also add the ship's and Bob's lights to its spot lights (Saucer::getLights,
// Bob::getLight), and register the ship as an Interactable.
struct AlienVisit {
  std::shared_ptr<Saucer> saucer;
  std::shared_ptr<Bob> bob;

  static AlienVisit create(Stage &stage, const glm::vec3 &landing, float rampYaw,
                           std::function<bool()> night,
                           std::function<bool(const glm::vec3 &, int, Bob::Victim &)> findVictim,
                           std::function<void(int, const glm::vec3 &)> abduct,
                           std::function<void(int, float)> paralyse,
                           std::function<void()> enterShip);
};

#endif
