#ifndef TEST_STAGE
#define TEST_STAGE

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "MaterialMap.h"
#include "NetRole.h"
#include "Mosquito.h"
#include "MosquitoEgg.h"
#include "Pingu.h"
#include "Readable.h"
#include "Satellite.h"
#include "VehicleStage.h"

using namespace glm;
using std::make_shared;

// The desert: dunes with a road winding through them, cacti and rocks, an RV,
// a satellite the player can orient, a sign, an NPC and the player: a penguin
// on foot, in first person. Using the RV's door (Use key) puts the penguin
// inside it and hands the controls and the camera (third person) to the RV;
// the leave-vehicle key (Left Shift) puts the penguin on foot at the door and
// goes back to first person. The rest (the RV, the day cycle, the controls)
// is VehicleStage's.
class TestStage : public VehicleStage {
private:
  // The giant mosquitoes: the first one and those born of its eggs, the eggs waiting to hatch, the
  // pool they are laid in, and how many there may be at most (eggs included)
  std::vector<std::shared_ptr<Mosquito>> mosquitoes;
  int eggsWaiting = 0;
  vec3 pool = vec3(0.0f);
  static constexpr int MAX_MOSQUITOES = 8;
  SoundEngine *soundEngine = nullptr;
  // The places round the player's head where young mosquitoes bite (shared by all of them)
  std::shared_ptr<Mosquito::BiteSlots> biteSlots = std::make_shared<Mosquito::BiteSlots>();

  // A mosquito at `where`, roaming round `home`, `growth` grown (0 = just hatched). `later`: while
  // the stage is updating (a hatching egg), it joins at the end of the frame
  std::shared_ptr<Mosquito> addMosquito(const vec3 &where, const vec3 &home, float growth, bool later,
                                        int netId = -1) {
    std::vector<std::shared_ptr<Model>> legSegments;
    for (int pair = 0; pair < 3; pair++)
      for (const char *side : {"L", "R"})
        for (int segment = 0; segment < 4; segment++)
          legSegments.push_back(loadModel("../assets/mosquito/mosquito_leg_" + std::string(side) +
                                          std::to_string(pair) + "_" + std::to_string(segment) + ".obj"));
    auto mosquito = make_shared<Mosquito>(loadModel("../assets/mosquito/mosquito.obj"),
                                          loadModel("../assets/mosquito/mosquito_abdomen.obj"),
                                          loadModel("../assets/mosquito/mosquito_wing_l.obj"),
                                          loadModel("../assets/mosquito/mosquito_wing_r.obj"),
                                          legSegments, *soundEngine);
    mosquito->setGrowth(growth);
    mosquito->setWaterSpots({pool});
    // It sucks the RV's fuel while the player is away from it, and bursts its tyres now and then
    // while he drives (and blows up doing it)
    mosquito->setVehicle(rv.get());
    mosquito->setHourQuery([this]() { return getTimeOfDay(); });
    mosquito->setHome(home);
    mosquito->setPosition(where.x, where.y, where.z);
    mosquito->setFloorQuery([this](float x, float z, float &height) { return floorAt(x, z, height); });
    // it goes for the nearest player alive (with nobody, it is as if he were dead)
    Mosquito *self = mosquito.get();
    mosquito->setTarget([this, self]() {
      Player *p = nearestAlive(self->getPosition());
      return p ? p->getPosition() : self->getPosition();
    });
    mosquito->setPlayerInVehicleQuery([this, self]() {
      Player *p = nearestAlive(self->getPosition());
      return p && playerSheltered(*p);
    });
    mosquito->setPlayerCaughtCallback([this, self]() {
      if (netRole() == NetRole::Client)
        return; // (the server says who dies)
      if (Player *p = nearestAlive(self->getPosition()))
        killPlayer(*p);
    });
    mosquito->setPlayerDeadQuery([this, self]() { return nearestAlive(self->getPosition()) == nullptr; });
    mosquito->setEggLayer([this](const vec3 &at) { return layEgg(at); });
    mosquito->setBiteSlots(biteSlots); // (several can bite the player at once)
    for (const auto &emitter : mosquito->getEmitters())
      addEmitter(emitter);
    mosquitoes.push_back(mosquito);
    if (later)
      addDynamicLater(mosquito);
    else
      addDynamic(mosquito, netId);
    return mosquito;
  }

  // An egg floating on the water at `at`; in a few seconds it hatches into a young mosquito. False
  // if there are as many mosquitoes and eggs as there may be
  bool layEgg(const vec3 &at) {
    int alive = eggsWaiting;
    for (const auto &m : mosquitoes)
      if (!m->isDead())
        alive++;
    if (alive >= MAX_MOSQUITOES)
      return false;
    eggsWaiting++;
    auto egg = make_shared<MosquitoEgg>(loadModel("../assets/mosquito/mosquito_egg.obj"), [this](MosquitoEgg &e) {
      eggsWaiting--;
      vec3 p = e.getPosition();
      addMosquito(p + vec3(0.0f, Mosquito::MIN_CLEARANCE * Mosquito::BABY_SCALE + 0.05f, 0.0f), p, 0.0f, true);
      removeLater(&e);
    });
    egg->setPosition(at.x, pool.y + 0.04f, at.z);
    egg->setYaw((float)std::fmod(at.x * 12.9898f + at.z * 78.233f, 6.2831853f)); // (any way round)
    addDynamicLater(egg);
    return true;
  }
  // Pingu: an NPC at (where.x, where.z) facing `lookAt`, who talks to the player (Use key)
  std::shared_ptr<Pingu> addPingu(const vec3 &where, const vec3 &lookAt, SoundEngine &sound,
                                  SpeechSynthesizer &speech) {
    VoiceSettings voice;
    voice.pitch = 62; // a bit higher than the default
    // Pingu dances; while he talks to the player he stands still, breathing
    // calmly (the same model in its idle pose): both are loaded
    auto dancing = make_shared<AnimatedModel>(
        "../assets/ping/PenguinoAnimado.fbx", true, PENGUIN_ANIMATION);
    auto standing = make_shared<AnimatedModel>(
        "../assets/ping/PenguinoAnimado.fbx", true, PENGUIN_ANIMATION);
    standing->setIdle(true);
    auto guide = make_shared<Pingu>(
        dancing, standing,
        "Pingu", std::vector<std::string>{
            "¡Hola, viajero! Soy Pingu y vigilo esta antena en mitad del desierto.",
            "Acércate al satélite y úsalo: puedes girarlo en azimut y en cénit para apuntar a cualquier punto del cielo.",
            "Dicen que de noche este desierto cambia por completo. Yo, por si acaso, me quedo aquí.",
        },
        sound, speech, voice);
    guide->setPosition(where.x, groundAt(where.x, where.z), where.z);
    guide->faceTowards(lookAt);
    guide->setGravity(25.0f);
    addDynamic(guide);
    interactables.push_back(guide.get());
    return guide;
  }

  static constexpr float GROUND_Y = -1.0f; // ground level of the clearing
  // What is within this many metres of the edge of the terrain is not drawn
  static constexpr float EDGE_CULL_MARGIN = 12.0f;

public:
  // Map editing: more mosquitoes and more Pingus than the map brings
  std::vector<std::string> entityKinds() const override {
    return {"folla_culos", "mosquito", "pingu", "gnome"};
  }
  bool spawnEntity(const std::string &kind, const vec3 &where, float yaw, EntityContext &context) override {
    if (kind == "mosquito") {
      vec3 nest(where.x, groundAt(where.x, where.z), where.z);
      addMosquito(nest + vec3(0.0f, 5.0f, 0.0f), nest, 1.0f, false)->setBlood(1.0f);
      return true;
    }
    if (kind == "pingu") {
      auto pingu = addPingu(where, where + vec3(std::sin(yaw), 0.0f, std::cos(yaw)), context.sound, context.speech);
      pingu->setYaw(yaw);
      return true;
    }
    return VehicleStage::spawnEntity(kind, where, yaw, context);
  }

  // A client: the server's mosquitoes and eggs that appear while the game runs
  bool spawnReplica(unsigned char kind, int netId, const vec3 &where, float arg) override {
    if (kind == NET_MOSQUITO) {
      addMosquito(where, where, arg, false, netId)->setReplica(true);
      return true;
    }
    if (kind == NET_EGG) {
      auto egg = make_shared<MosquitoEgg>(loadModel("../assets/mosquito/mosquito_egg.obj"), nullptr);
      egg->setPosition(where.x, where.y, where.z);
      addDynamic(egg, netId);
      egg->setReplica(true);
      return true;
    }
    return false;
  }

  void getSpotLights(std::vector<SpotLight> &lights) const override {
    VehicleStage::getSpotLights(lights);
    for (const auto &mosquito : mosquitoes) // the flash of an explosion (only for a moment)
      mosquito->getLight(lights);
  }

  // The NPCs speak through `sound` with voices made by `speech`
  TestStage(FloorMode mode, SoundEngine &sound, SpeechSynthesizer &speech)
      : VehicleStage(mode) {
    soundEngine = &sound;
    groundFallback = GROUND_Y;
    // The desert's background music: an arid guitar and banjo loop
    loadMusic("../assets/music/desert.wav");
    // ...and the wind, which never stops (it is all that is left at night)
    loadAmbience("../assets/music/wind.wav");

    startDay();
    setEdgeCulling(EDGE_CULL_MARGIN); // (once the floor is known)
    // First person: the camera at the penguin's eyes, 1.6 above its feet

    // Desert scenery
    auto ground = make_shared<GameObject>(loadModel("../assets/desert/dunes_loop.obj"));
    ground->setPosition(0.0f, GROUND_Y, 0.0f);
    ground->setCollidable(false); // it is the floor, not an obstacle
    add(ground);
    // The dunes are a regular grid of heights, so they are the height map
    // and a height field needs a material map: asphalt where the road is, sand
    // everywhere else (the RV is slower on sand)
    auto materials =
        MaterialMap::loadImage("../assets/desert/dunes_loop_materials.png");
    if (!materials) {
      fprintf(stderr, "No material map: the whole floor is sand\n");
      materials = MaterialMap::uniform(FloorMaterial::Sand);
    }
    setFloor(loadModel("../assets/desert/dunes_loop.obj"),
             vec3(0.0f, GROUND_Y, 0.0f), materials);
    // The road: 8 m wide, a closed loop winding round the starting clearing
    // (about 250 m long), carved into the dunes
    auto road = make_shared<GameObject>(loadModel("../assets/desert/road.obj"));
    road->setPosition(0.0f, GROUND_Y, 0.0f);
    road->setCollidable(false);
    add(road);

    // model, x, z, rotation around y, uniform scale. Scattered by a script
    // (seeded) over the dunes, never on the road (at least 7 m from its centre
    // line) nor in the starting clearing: 14 inside the loop, 30 outside it
    struct Prop {
      const char *model;
      float x, z, yaw, scale;
    };
    const char *cactusA = "../assets/desert/cactus_a.obj";
    const char *cactusB = "../assets/desert/cactus_b.obj";
    const char *rockA = "../assets/desert/rock_a.obj";
    const char *rockB = "../assets/desert/rock_b.obj";
    const Prop propList[] = {
        {rockA, 7.1f, -18.7f, 0.3f, 1.3f},
        {cactusA, -13.3f, 8.6f, 6.3f, 0.9f},
        {cactusB, 1.8f, 18.4f, 5.6f, 1.8f},
        {cactusA, -7.5f, -15.2f, 1.7f, 1.4f},
        {rockA, 19.1f, 8.2f, 1.4f, 2.2f},
        {cactusA, 14.6f, -15.1f, 0.0f, 1.5f},
        {rockB, -6.4f, 12.5f, 0.9f, 1.4f},
        {cactusB, -17.1f, -0.7f, 5.9f, 0.9f},
        {cactusB, 7.2f, -10.6f, 2.5f, 1.8f},
        {cactusB, -20.0f, -7.4f, 4.2f, 1.6f},
        {rockB, 16.9f, -6.6f, 5.1f, 2.0f},
        {cactusB, -8.9f, 19.7f, 2.6f, 0.9f},
        {rockB, 8.4f, 20.2f, 2.5f, 1.1f},
        {rockA, 13.0f, 15.6f, 3.1f, 1.3f},
        {rockB, 8.7f, -64.6f, 2.8f, 1.3f},
        {cactusA, -72.6f, -81.5f, 5.8f, 1.2f},
        {rockA, 63.9f, -3.2f, 5.4f, 1.9f},
        {cactusB, -26.7f, 48.2f, 4.6f, 1.4f},
        {cactusA, 32.4f, -81.1f, 1.3f, 1.0f},
        {cactusA, -48.7f, -74.0f, 5.6f, 1.4f},
        {cactusB, 46.8f, 72.3f, 2.1f, 1.5f},
        {rockB, 53.1f, -44.2f, 4.6f, 1.7f},
        {cactusA, -0.9f, -73.7f, 3.0f, 1.4f},
        {rockA, 54.6f, -78.7f, 3.8f, 1.6f},
        {rockB, -50.7f, 80.4f, 1.8f, 2.3f},
        {cactusA, -41.5f, 59.2f, 1.6f, 1.5f},
        {rockA, -71.2f, -50.4f, 3.4f, 1.7f},
        {cactusA, -1.9f, 69.3f, 1.6f, 1.6f},
        {cactusA, -50.3f, -28.9f, 2.3f, 1.7f},
        {cactusA, -53.2f, -82.0f, 2.6f, 1.1f},
        {rockB, 16.7f, 78.9f, 3.8f, 1.3f},
        {rockB, 69.1f, -65.3f, 4.9f, 1.5f},
        {rockA, 42.6f, 53.0f, 6.0f, 1.5f},
        {cactusB, -20.6f, 62.3f, 2.3f, 1.2f},
        {cactusB, -72.2f, -36.5f, 3.4f, 1.0f},
        {cactusA, -9.5f, -77.9f, 0.8f, 1.7f},
        {cactusA, -20.8f, 73.9f, 5.0f, 1.3f},
        {rockB, -78.9f, -70.9f, 1.6f, 1.9f},
        {cactusA, 64.4f, 73.8f, 1.2f, 1.4f},
        {cactusB, 35.0f, -50.9f, 2.4f, 1.3f},
        {cactusB, 29.9f, 47.3f, 0.4f, 1.3f},
        {rockA, -30.1f, 63.4f, 0.9f, 2.3f},
        {cactusB, -60.6f, 2.4f, 5.1f, 1.2f},
        {rockB, -79.8f, 81.9f, 1.2f, 1.9f},
    };
    for (const Prop &p : propList) {
      auto o = make_shared<GameObject>(loadModel(p.model));
      // sink the base a little so nothing floats on the slopes
      o->setPosition(p.x, groundAt(p.x, p.z) - 0.05f, p.z);
      o->setYaw(p.yaw);
      o->setScale(p.scale);
      add(o);
    }

    // The creature, standing in the distance and facing the camera (disabled)
    // auto creature = make_shared<DynamicGameObject>(
    //     loadModel("../assets/creature/creature.obj"));
    // creature->addPart(loadModel("../assets/creature/creature_eyes.obj"),
    //                   2); // the eyes glow
    // creature->setPosition(1.5f, GROUND_Y + 0.91f, -13.0f); // see terrain_height()
    // creature->setYaw(0.25f);
    // creature->setBreathAmp(2.0f); // the creature breathes
    // addDynamic(creature);

    // The RV, facing +z: its door (+x side) is towards the start
    createRV(sound, 0.0f, 0.0f, 0.0f);
    createWalker(3.0f, 4.0f);

    // A satellite next to the start, within reach (see Interactable)
    auto satellite = make_shared<Satellite>(
        loadModel("../assets/antenna/antenna_dish.obj"),
        loadModel("../assets/antenna/antenna_base.obj"),
        vec3(4.5f, groundAt(4.5f, 2.0f), 2.0f));
    add(satellite);
    add(satellite->getMount());
    interactables.push_back(satellite.get());

    // An NPC a few steps ahead of the start, facing it: talk to it with the
    // Use key. Same model as the player, standing on its feet.
    auto guide = addPingu(vec3(3.0f, 0.0f, 0.5f), vec3(3.0f, 0.0f, 4.0f), sound, speech);

    createCreature(sound, speech, 18.0f, 24.0f);
    // The other NPCs are its prey if they come near
    Npc *pingu = guide.get();
    creature->setPreyQuery([pingu]() { return std::vector<Npc *>{pingu}; });

    // A pool of water (for now a blue square, flat on a flat bit of sand): the mosquito lays its
    // eggs in it
    pool = vec3(-44.0f, 0.0f, 67.0f);
    pool.y = std::max(std::max(groundAt(pool.x - 2.0f, pool.z - 2.0f), groundAt(pool.x + 2.0f, pool.z - 2.0f)),
                      std::max(groundAt(pool.x - 2.0f, pool.z + 2.0f), groundAt(pool.x + 2.0f, pool.z + 2.0f)));
    auto puddle = make_shared<GameObject>(loadModel("../assets/water/puddle.obj"));
    puddle->setPosition(pool.x, pool.y + 0.03f, pool.z);
    puddle->setCollidable(false); // (before add: a static is registered when added)
    add(puddle);

    // A giant mosquito: it roams its corner of the desert looking for water to lay its eggs in,
    // and at dawn and at dusk, when the player comes near, it circles him and dives to bite (see
    // Mosquito). Running it over with the RV kills it. Its eggs hatch into more (layEgg).
    vec3 nest(-35.0f, 0.0f, 40.0f);
    nest.y = groundAt(nest.x, nest.z);
    // (it starts with blood in its stomach: it can lay its eggs)
    addMosquito(nest + vec3(0.0f, 5.0f, 0.0f), nest, 1.0f, false)->setBlood(1.0f);

    // A sign to read (no voice: the text types itself out), past the
    // satellite, turned towards the start
    auto sign = make_shared<Readable>(
        loadModel("../assets/sign/sign.obj"), "Cartel",
        std::vector<std::string>{
            "AVISO: estación de seguimiento del desierto. Prohibido el paso a personal no autorizado.",
            "La antena se orienta con el azimut y el cénit. No la apuntéis nunca directamente al sol.",
            "Si de noche veis algo moverse entre las dunas, no os acerquéis. Volved a la carretera.",
        },
        1.3f); // the board's height
    sign->setPosition(7.5f, groundAt(7.5f, -1.0f), -1.0f);
    sign->setYaw(std::atan2(3.0f - 7.5f, 4.0f + 1.0f)); // face (3, 4)
    add(sign);
    interactables.push_back(sign.get());

    // Bob's ship comes at night and lands on a flat bit of sand off the road, 25 m from the start
    // (its ground varies 0.23 m within 5 m; the road is 15 m away, the nearest cactus or rock 9 m),
    // its ramp towards the start
    vec3 landing(-18.0f, 0.0f, 18.0f);
    landing.y = groundAt(landing.x, landing.z);
    createAlienVisit(sound, landing, std::atan2(3.0f - landing.x, 4.0f - landing.z));

    // A house (see House) on the flattest sand there is 50 m from the start (the ground varied 0.8 m
    // under it: placeHouse levels it), 13 m from the road and 6 m from the nearest cactus or rock, its
    // porch towards the start
    placeHouse(9.0f, 57.0f, std::atan2(-(4.0f - 57.0f), 3.0f - 9.0f));

    // A garden gnome a few steps from the start, facing it (see Gnome: do not look at it too much)
    createGnome(-8.0f, 8.0f, std::atan2(3.0f + 8.0f, 4.0f - 8.0f));
  }
};

#endif
