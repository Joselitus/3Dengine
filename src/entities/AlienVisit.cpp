#include "AlienVisit.h"

#include "Stage.h"

using namespace glm;
using std::shared_ptr;
using std::string;

AlienVisit AlienVisit::create(Stage &stage, const vec3 &landing, float rampYaw,
                              std::function<bool()> night, std::function<vec3()> player,
                              std::function<bool()> inVehicle, std::function<bool()> playerDead,
                              std::function<void(const vec3 &)> abduct,
                              std::function<void(float)> paralyse, std::function<bool()> paralysed,
                              std::function<void()> enterShip) {
  const string dir = "../assets/bob/";
  AlienVisit visit;
  visit.saucer = std::make_shared<Saucer>(
      stage.loadModel(dir + "saucer_hull.obj"), stage.loadModel(dir + "saucer_lights.obj"),
      stage.loadModel(dir + "saucer_legs.obj"), stage.loadModel(dir + "saucer_ramp.obj"),
      stage.loadModel(dir + "saucer_beam.obj"), stage.loadModel(dir + "saucer_gun_mount.obj"),
      stage.loadModel(dir + "saucer_gun.obj"), stage.loadModel(dir + "saucer_shot.obj"));
  std::vector<shared_ptr<Model>> limbs;
  for (const char *side : {"l", "r"})
    for (const char *limb : {"upperarm", "forearm", "thigh", "shin"})
      limbs.push_back(stage.loadModel(dir + "bob_" + limb + "_" + side + ".obj"));
  visit.bob = std::make_shared<Bob>(stage.loadModel(dir + "bob_body.obj"),
                                    stage.loadModel(dir + "bob_eyes.obj"),
                                    stage.loadModel(dir + "bob_eyes_glow.obj"), limbs,
                                    stage.loadModel(dir + "bob_ray.obj"));
  visit.saucer->setLanding(landing, rampYaw);
  visit.saucer->setNightQuery(night);
  visit.saucer->setBob(visit.bob);
  visit.saucer->setEnterAction(enterShip);
  Saucer *ship = visit.saucer.get();
  visit.bob->setShip(ship);
  visit.bob->setNightQuery(night);
  visit.bob->setTarget(player);
  visit.bob->setPlayerInVehicleQuery(inVehicle);
  visit.bob->setPlayerDeadQuery(playerDead);
  visit.bob->setTakePlayerCallback([abduct, ship]() { abduct(ship->hatch()); });
  visit.bob->setParalyseCallback(paralyse);
  visit.bob->setPlayerParalysedQuery(paralysed);
  visit.bob->setPosition(landing.x, landing.y, landing.z);
  stage.addDynamic(visit.saucer);
  for (auto &emitter : visit.saucer->getEmitters())
    stage.addEmitter(emitter);
  stage.addDynamic(visit.bob);
  return visit;
}
