#include "VehicleStage.h"

#include <cmath>

#include "FuelPump.h"
#include "NetRole.h"

using namespace glm;
using std::make_shared;

constexpr unsigned int VehicleStage::PENGUIN_ANIMATION;

Player *VehicleStage::nearestAlive(const vec3 &from) {
  Player *best = nullptr;
  float bestDistance = 0.0f;
  for (auto &p : players) {
    if (p->dead)
      continue;
    float d = length(p->getPosition() - from);
    if (!best || d < bestDistance) {
      best = p.get();
      bestDistance = d;
    }
  }
  return best;
}

void VehicleStage::enterRV() {
  if (!acting)
    return;
  Player &p = *acting;
  if (p.inVehicle || p.inSaucer || p.dead || rv->isOccupied())
    return;
  p.inVehicle = true;
  p.walker->control(vec2(0.0f), 0.0f, 0.0f); // stops walking
  p.walker->setVelocity(vec3(0.0f));
  p.walker->setGravity(0.0f);       // it rides: nothing pulls it down
  p.walker->setCollidable(false);   // inside the RV's box
  p.walker->setVisible(false);
  vec3 seat = rv->seatPosition();
  p.walker->setPosition(seat.x, seat.y, seat.z);
  rv->setOccupied(true);
  setControl(p, rv, CAR_CAMERA_DISTANCE, CAR_CAMERA_HEIGHT, rv->headingYaw());
}

void VehicleStage::enterSaucer() {
  if (!acting || !alien.saucer)
    return;
  Player &p = *acting;
  if (p.inVehicle || p.inSaucer || p.dead || alien.saucer->isPiloted())
    return;
  p.inSaucer = true;
  p.walker->control(vec2(0.0f), 0.0f, 0.0f);
  p.walker->setVelocity(vec3(0.0f));
  p.walker->setGravity(0.0f);
  p.walker->setCollidable(false);
  p.walker->setVisible(false);
  alien.saucer->setPiloted(true);
  vec3 to = alien.saucer->getPosition() - p.walker->getPosition();
  setControl(p, alien.saucer, Saucer::CAMERA_DISTANCE, Saucer::CAMERA_HEIGHT, std::atan2(to.x, -to.z));
}

void VehicleStage::leaveVehicle(Player &p) {
  if (p.dead)
    return;
  if (p.inSaucer) {
    if (!alien.saucer->canDisembark())
      return; // (in the air, or on its belly: the key brings it down)
    p.inSaucer = false;
    alien.saucer->setPiloted(false);
    vec3 foot = alien.saucer->rampFoot();
    p.walker->setPosition(foot.x, groundAt(foot.x, foot.z), foot.z);
    p.walker->setVelocity(vec3(0.0f));
    p.walker->setGravity(AVATAR_GRAVITY);
    p.walker->setCollidable(true);
    p.walker->setVisible(true);
    vec3 away = foot - alien.saucer->getPosition();
    setControl(p, p.walker, 0.0f, EYE_HEIGHT, std::atan2(away.x, -away.z)); // (looking away from it)
    return;
  }
  if (!p.inVehicle) {
    if (alien.bob)
      alien.bob->struggleOnce(p.id); // (held by Bob: he fights to get free)
    return;
  }
  if (std::fabs(rv->forwardSpeed()) > 2.0f)
    return; // (not while it moves: the penguin would be left inside it as it drives off)
  p.inVehicle = false;
  rv->control(vec2(0.0f), 0.0f, 0.0f); // the RV stops being driven
  rv->setOccupied(false);
  // Out of the seat, onto the floor of the cab behind the wheel (the way out is the door)
  vec3 stand = rv->driverStand();
  p.walker->setPosition(stand.x, stand.y, stand.z);
  p.walker->setVelocity(vec3(0.0f));
  p.walker->setGravity(AVATAR_GRAVITY);
  p.walker->setCollidable(true);
  p.walker->setVisible(true);
  // it looks forward, along the RV
  setControl(p, p.walker, 0.0f, EYE_HEIGHT, rv->headingYaw());
}

// A player who dies or leaves lets go of what he drives: the RV is free, the ship is left to land
void VehicleStage::onPlayerGone(Player &p) {
  if (p.inVehicle) {
    p.inVehicle = false;
    rv->control(vec2(0.0f), 0.0f, 0.0f);
    rv->setOccupied(false);
    vec3 stand = rv->driverStand();
    p.walker->setPosition(stand.x, stand.y, stand.z);
  } else if (p.inSaucer) {
    p.inSaucer = false;
    if (alien.saucer) {
      alien.saucer->control(vec2(0.0f), 0.0f, 0.0f);
      vec3 foot = alien.saucer->rampFoot();
      p.walker->setPosition(foot.x, groundAt(foot.x, foot.z), foot.z);
    }
  }
  p.walker->setVelocity(vec3(0.0f));
  p.walker->setGravity(AVATAR_GRAVITY);
  p.walker->setVisible(true);
  p.paralysis = 0.0f;
  setControl(p, p.walker, 0.0f, EYE_HEIGHT, p.cameraYaw);
}

void VehicleStage::respawn(Player &p) {
  GameStage::respawn(p);
  p.paralysis = 0.0f;
}

bool VehicleStage::isImmobilized(const Player &p) const {
  return p.paralysis > 0.0f || (alien.bob && alien.bob->isHolding(p.id));
}

// The ship nobody flies: the engine stops, and once it stands on its legs it is Bob's again
void VehicleStage::onTick(double dt) {
  if (!alien.saucer || !alien.saucer->isPiloted())
    return;
  bool piloted = false;
  for (auto &p : players)
    if (p->inSaucer && !p->dead) {
      piloted = true;
      // (the ray gun follows where he looks)
      float cp = std::cos(p->lookPitch);
      alien.saucer->setAim(vec3(cp * std::sin(p->lookYaw), -std::sin(p->lookPitch), -cp * std::cos(p->lookYaw)));
    }
  if (piloted)
    return;
  if (alien.saucer->isEngineOn())
    alien.saucer->toggleEngine();
  if (alien.saucer->canDisembark())
    alien.saucer->setPiloted(false);
}

void VehicleStage::apply(DynamicGameObject &object, double dt) {
  for (auto &p : players) {
    if (&object != p->walker.get())
      continue;
    p->paralysis = std::max(0.0f, p->paralysis - (float)dt); // (it wears off)
    if (p->inVehicle || p->inSaucer) {
      vec3 seat = p->inSaucer ? alien.saucer->hatch() : rv->seatPosition();
      object.setPosition(seat.x, seat.y, seat.z);
      object.setVelocity(vec3(0.0f));
      return;
    }
    break;
  }
  collideWithFloor(object, dt);
}

void VehicleStage::toggleHeadlights(Player &p) {
  if (p.inSaucer) {
    alien.saucer->toggleGun(); // (the ship has no lights: the key brings out its ray gun)
    return;
  }
  if (p.inVehicle)
    rv->toggleHeadlights();
  else
    p.walker->toggleFlashlight();
}

void VehicleStage::toggleEngine(Player &p) {
  if (p.inVehicle)
    rv->toggleEngine();
  else if (p.inSaucer)
    alien.saucer->toggleEngine();
}

void VehicleStage::toggleShipLegs(Player &p) {
  if (p.inSaucer)
    alien.saucer->toggleLegs();
}

void VehicleStage::fire(Player &p, const vec3 &eye, const vec3 &direction) {
  // (the shot starts at the ship's gun: the client's word for where it is is not taken)
  if (p.inSaucer)
    alien.saucer->fire(*this, alien.saucer->gunPivot(), direction);
}

float VehicleStage::playerParalysis() const {
  return local ? std::min(1.0f, local->paralysis / Bob::PARALYSIS_TIME) : 0.0f;
}

float VehicleStage::struggleProgress() const {
  return alien.bob && local ? alien.bob->struggleProgress(local->id) : -1.0f;
}

float VehicleStage::alienPresence() const { return alien.bob ? alien.bob->getPresence() : 0.0f; }

void VehicleStage::endAlienHiss() {
  if (alien.bob)
    alien.bob->silenceHiss();
}

// The RV's two side mirrors, while somebody drives it
bool VehicleStage::rearMirror(int side, MirrorView &view) const {
  return local && local->inVehicle && rv->rearMirror(side, view);
}

void VehicleStage::setRearMirrorTexture(int side, unsigned int texture, float aspect) {
  rv->setMirrorTexture(side, texture, aspect);
}

void VehicleStage::showRearMirror(int side, bool show) { rv->showMirror(side, show); }

void VehicleStage::toggleHandbrake(Player &p) {
  if (p.inVehicle)
    rv->toggleHandbrake();
}

void VehicleStage::toggleVehicleCamera(Player &p) {
  if (p.inVehicle)
    rv->toggleCameraView();
}

void VehicleStage::refuel(Player &p) {
  // (at a pump, with the RV parked by it: the pump checked the distance on the client, and the
  // server checks it again)
  if (p.dead || !rv)
    return;
  for (Interactable *target : interactables)
    if (dynamic_cast<FuelPump *>(target) && length(p.getPosition() - target->getInteractionPoint()) < 8.0f &&
        length(rv->getPosition() - target->getInteractionPoint()) < 16.0f) {
      rv->setFuel(1.0f);
      return;
    }
}

void VehicleStage::getSpotLights(std::vector<SpotLight> &lights) const {
  GameStage::getSpotLights(lights); // (the players' flashlights, the local one first)
  if (alien.saucer)
    alien.saucer->getLights(lights); // (Bob's ship: its beam)
  if (alien.bob)
    alien.bob->getLight(lights);     // (his ray)
  rv->getHeadlights(lights);
  rv->getFireLight(lights);
  rv->getDashboardLights(lights);
  for (const auto &c : creatures)
    c->getLight(lights); // (last: if the shader has no room, the creature's is the one left out)
}

void VehicleStage::onTimeChanged() {
  // 0 below `a`, 1 above `b`, smooth between (also works if a > b)
  auto ramp = [](float a, float b, float x) {
    float t = clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
  };
  float angle = (getTimeOfDay() - 6.0f) / 24.0f * 6.2831853f;
  vec3 sun = normalize(vec3(cos(angle), sin(angle), -0.3f));
  float h = sun.y; // how high the sun is (negative: below the horizon)

  float day = ramp(-0.1f, 0.3f, h);
  float dusk = ramp(-0.2f, 0.0f, h) * (1.0f - ramp(0.05f, 0.35f, h));
  vec3 horizon = mix(vec3(0.035f, 0.055f, 0.11f), vec3(0.45f, 0.68f, 0.92f), day);
  horizon = mix(horizon, vec3(0.95f, 0.52f, 0.30f), dusk * 0.85f);
  vec3 zenith = mix(vec3(0.003f, 0.007f, 0.028f), vec3(0.16f, 0.38f, 0.78f), day);
  zenith = mix(zenith, vec3(0.25f, 0.22f, 0.45f), dusk * 0.5f);
  environment.horizon = horizon;
  environment.skyZenith = zenith;
  environment.sunDir = sun;
  environment.starAlpha = 1.0f - ramp(-0.2f, 0.0f, h);

  // The sun lights the world while it is up; at night there is only the
  // minimum that is always there, from straight above
  vec3 sunColor = mix(vec3(1.0f, 0.55f, 0.25f), vec3(0.85f, 0.83f, 0.78f),
                      ramp(0.0f, 0.45f, h));
  float sunPower = ramp(-0.05f, 0.25f, h);
  environment.lightColor = sunColor * sunPower + NIGHT_LIGHT;
  // The light comes from the sun while it gives noticeable light; only the
  // minimum that is left (NIGHT_LIGHT) comes from above, so as the sun sets
  // its share of the light shrinks instead of the direction drifting early
  float sunShare = sunPower / (sunPower + length(NIGHT_LIGHT));
  environment.lightDir = normalize(mix(vec3(0.0f, 1.0f, 0.0f), sun, sunShare));
  setMusicVolume(ramp(-0.1f, 0.5f, h)); // gone shortly after sunset
}

void VehicleStage::startDay() {
  // A day lasts DAY_DURATION seconds; the sky and the light follow the
  // clock (see onTimeChanged)
  setSky(loadModel("../assets/sky/skydome_plain.obj"), 3);
  setDayDuration(DAY_DURATION);
  setTimeOfDay(START_HOUR);
}

void VehicleStage::createRV(SoundEngine &sound, float x, float z, float heading) {
  // The RV (front toward +z, wheels on y = 0)
  rv = make_shared<RV>(loadModel("../assets/rv/rv.obj"));
  rv->setPosition(x, groundAt(x, z), z);
  rv->setHeading(heading); // 0: facing +z, its door (+x side) towards the start
  rv->setHandbrakeOn(true); // parked: the lever is up (nothing else holds it on a slope)
  // The wheels are separate models so they follow the suspension
  rv->setWheelModels(loadModel("../assets/rv/wheel_negx.obj"),
                     loadModel("../assets/rv/wheel_posx.obj"));
  // The windshield: intact, and the cracked one that replaces it after a crash
  rv->setWindshieldModels(loadModel("../assets/rv/windshield.obj"),
                          loadModel("../assets/rv/windshield_broken.obj"));
  // The cockpit: the dashboard, the ignition key and the gauges' needle
  rv->setCockpitModels(loadModel("../assets/rv/dashboard.obj"),
                       loadModel("../assets/rv/key.obj"),
                       loadModel("../assets/rv/needle.obj"),
                       loadModel("../assets/rv/dashboard_glow.obj"));
  rv->setSteeringWheelModel(loadModel("../assets/rv/steering_wheel.obj"));
  rv->setEngineSound(sound);
  rv->setHeadlightGlowModel(loadModel("../assets/rv/headlight_glow.obj"));
  rv->setHandbrakeModels(loadModel("../assets/rv/handbrake_base.obj"),
                         loadModel("../assets/rv/handbrake_lever.obj"));
  rv->setDoorModel(loadModel("../assets/rv/door.obj"));
  rv->setAlarmLampModel(loadModel("../assets/rv/dashboard_alarm.obj"));
  rv->setMaxSpeed(20.0f);
  rv->setGravity(25.0f);
  rv->setStage(this);
  addDynamic(rv);
  // The dust its wheels throw up on sand (the stage moves and removes it)
  for (const auto &emitter : rv->getDust())
    addEmitter(emitter);
  for (const auto &emitter : rv->getGrains())
    addEmitter(emitter);
  // The fire of a wrecked front and the explosion of its engine (a blast kills whoever is in
  // the RV or close to it)
  for (const auto &emitter : rv->getFireEmitters())
    addEmitter(emitter);
  for (const auto &emitter : rv->getBlastEmitters())
    addEmitter(emitter);
  rv->setExplosionCallback([this](const vec3 &centre, float radius) {
    if (netRole() == NetRole::Client)
      return; // (the server says who dies)
    for (auto &p : players) {
      if (p->dead)
        continue;
      vec3 at = p->inVehicle ? centre : p->walker->getPosition() + vec3(0.0f, 0.9f, 0.0f);
      if (length(at - centre) < radius)
        killPlayer(*p);
    }
  });
  // Its door opens and closes (E next to it); to drive, the penguin walks in and uses the steering
  // wheel (see enterRV)
  rv->setEnterAction([this]() { enterRV(); });
  interactables.push_back(rv.get());
  interactables.push_back(rv->steeringInteraction());
}

void VehicleStage::createWalker(float x, float z, float yaw) {
  // The players' penguins appear here, on foot, their feet on the floor (see GameStage::addPlayer)
  spawnPoint = vec3(x, groundAt(x, z), z);
  spawnYaw = yaw;
}

void VehicleStage::createCreature(SoundEngine &sound, SpeechSynthesizer &speech, float x,
                                  float z) {
  // The night creature: it runs straight at whoever the player controls (the penguin on
  // foot, or the RV when driving). The model is its own size, in metres.
  creature = make_shared<FollaCulos>(
      make_shared<AnimatedModel>("../assets/folla_culos/folla_culos_run.glb", true),
      make_shared<AnimatedModel>("../assets/folla_culos/folla_culos_run.glb", false, 1),
      sound, speech);
  // If it is run over from the front it ends up stuck on the windshield of the RV
  creature->setFrontHitTest([this](const FollaCulos &c) {
    return rv->forwardSpeed() > 1.0f && rv->isInFront(c.getPosition());
  });
  creature->setRagdollWorld(
      [this](float x, float z, float &height) { return floorAt(x, z, height); },
      [this](vec3 &point, float radius) { rv->pushOutOfBody(point, radius); },
      [this]() { return rv->getVelocity(); });
  creature->setSurfaceFrame([this](vec3 &center, vec3 &up, vec3 &normal) {
    rv->windshieldFrame(center, up, normal);
  });
  creature->setPosition(x, groundAt(x, z), z);
  creature->setGravity(25.0f);
  // It goes for the nearest player alive (and, with nobody, stays put as if he were dead)
  FollaCulos *self = creature.get();
  creature->setTarget([this, self]() {
    Player *p = nearestAlive(self->getPosition());
    return p ? p->getPosition() : self->getPosition();
  });
  // Touching a player on foot kills him; then it runs away
  creature->setPlayerCaughtCallback([this, self]() {
    if (netRole() == NetRole::Client)
      return; // (the server says who dies)
    if (Player *p = nearestAlive(self->getPosition()))
      killPlayer(*p);
  });
  creature->setPlayerDeadQuery([this, self]() { return nearestAlive(self->getPosition()) == nullptr; });
  // It is wary of the RV: when the player drives it, it keeps its distance (see FollaCulos)
  creature->setPlayerInVehicleQuery([this, self]() {
    Player *p = nearestAlive(self->getPosition());
    return p && p->inVehicle;
  });
  // Its face follows the camera
  creature->setLookTarget([this]() { return viewer; });
  // At night it comes for you; by day it keeps away (the sun is below the horizon)
  creature->setNightQuery([this]() { return environment.sunDir.y < 0.0f; });
  addDynamic(creature);
  creatures.push_back(creature);
}

void VehicleStage::createAlienVisit(SoundEngine &sound, const vec3 &landing, float rampYaw) {
  alien = AlienVisit::create(
      *this, landing, rampYaw, [this]() { return environment.sunDir.y < 0.0f; },
      // whom Bob goes for: the player he already has, else the nearest one alive
      [this](const vec3 &from, int keep, Bob::Victim &victim) {
        Player *chosen = nullptr;
        float bestDistance = 0.0f;
        for (auto &p : players) {
          if (p->dead)
            continue;
          float d = length(p->getPosition() - from);
          if (p->id == keep) {
            chosen = p.get();
            break;
          }
          if (!chosen || d < bestDistance) {
            chosen = p.get();
            bestDistance = d;
          }
        }
        if (!chosen)
          return false;
        victim.id = chosen->id;
        victim.position = chosen->getPosition();
        victim.inVehicle = chosen->inVehicle || chosen->inSaucer;
        victim.paralysed = chosen->paralysis > 0.0f;
        return true;
      },
      [this](int id, const vec3 &into) {
        if (Player *p = findPlayer(id))
          abductPlayer(*p, into);
      },
      [this](int id, float seconds) {
        if (Player *p = findPlayer(id))
          p->paralysis = std::max(p->paralysis, seconds);
      },
      [this]() { enterSaucer(); });
  alien.bob->setSounds(sound);
  alien.saucer->setSounds(sound);
  alien.bob->setViewer([this]() { return viewer; }, [this]() { return viewProjection; });
  // (the film grain and the hiss are for the player at this computer)
  alien.bob->setViewerState([this]() { return local && local->abducted; },
                            [this]() { return local && local->dead; });
  interactables.push_back(alien.saucer.get()); // (its ramp: get in)
}
