#include "VehicleStage.h"

#include <cmath>

using namespace glm;
using std::make_shared;

constexpr unsigned int VehicleStage::PENGUIN_ANIMATION;

void VehicleStage::enterRV() {
  if (inVehicle)
    return;
  inVehicle = true;
  walker->control(vec2(0.0f), 0.0f, 0.0f); // stops walking
  walker->setVelocity(vec3(0.0f));
  walker->setGravity(0.0f);       // it rides: nothing pulls it down
  walker->setCollidable(false);   // inside the RV's box
  walker->setVisible(false);
  vec3 seat = rv->seatPosition();
  walker->setPosition(seat.x, seat.y, seat.z);
  rv->setOccupied(true);
  setPlayer(rv, CAR_CAMERA_DISTANCE, CAR_CAMERA_HEIGHT, rv->headingYaw());
}

void VehicleStage::leaveVehicle() {
  if (!inVehicle)
    return;
  inVehicle = false;
  rv->control(vec2(0.0f), 0.0f, 0.0f); // the RV stops being driven
  rv->setOccupied(false);
  vec3 door = rv->doorPosition(1.5f); // beside the door, clear of the body
  walker->setPosition(door.x, groundAt(door.x, door.z), door.z);
  walker->setVelocity(vec3(0.0f));
  walker->setGravity(25.0f);
  walker->setCollidable(true);
  // (attaching it hides its model); it looks away from the RV
  setPlayer(walker, 0.0f, EYE_HEIGHT, rv->doorYaw());
}

void VehicleStage::apply(DynamicGameObject &object, double dt) {
  if (inVehicle && &object == walker.get()) {
    vec3 seat = rv->seatPosition();
    object.setPosition(seat.x, seat.y, seat.z);
    object.setVelocity(vec3(0.0f));
    return;
  }
  collideWithFloor(object, dt);
}

void VehicleStage::toggleHeadlights() {
  if (inVehicle)
    rv->toggleHeadlights();
  else
    walker->toggleFlashlight();
}

void VehicleStage::toggleEngine() {
  if (inVehicle)
    rv->toggleEngine();
}

void VehicleStage::toggleHandbrake() {
  if (inVehicle)
    rv->toggleHandbrake();
}

void VehicleStage::toggleVehicleCamera() {
  if (inVehicle)
    rv->toggleCameraView();
}

void VehicleStage::getSpotLights(std::vector<SpotLight> &lights) const {
  if (!inVehicle)
    walker->getFlashlight(lights); // (first: the shader may not have room for all)
  rv->getHeadlights(lights);
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
  rv->setMaxSpeed(20.0f);
  rv->setGravity(25.0f);
  addDynamic(rv);
  // The dust its wheels throw up on sand (the stage moves and removes it)
  for (const auto &emitter : rv->getDust())
    addEmitter(emitter);
  for (const auto &emitter : rv->getGrains())
    addEmitter(emitter);
  // Using its door gets the player in (see enterRV)
  rv->setEnterAction([this]() { enterRV(); });
  interactables.push_back(rv.get());
}

void VehicleStage::createWalker(float x, float z) {
  // The player: a penguin on foot (a Walker), its feet on the floor, seen
  // in first person. It is drawn centred on its position (AnimatedModel
  // fits it to 1.8 units around the origin), which only shows if the camera
  // is moved out of first person.
  walker = make_shared<Walker>(
      make_shared<AnimatedModel>("../assets/ping/PenguinoAnimado.fbx", false,
                                 PENGUIN_ANIMATION));
  walker->setPosition(x, groundAt(x, z), z);
  walker->setGravity(25.0f);
  addDynamic(walker);
  player = walker;
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
  creature->setTarget([this]() { return player->getPosition(); });
  // Touching the player on foot kills him; then it runs away
  creature->setPlayerCaughtCallback([this]() { killPlayer(); });
  creature->setPlayerDeadQuery([this]() { return isPlayerDead(); });
  // It is wary of the RV: when the player drives it, it keeps its distance (see FollaCulos)
  creature->setPlayerInVehicleQuery([this]() { return inVehicle; });
  // Its face follows the camera
  creature->setLookTarget([this]() { return viewer; });
  // At night it comes for you; by day it keeps away (the sun is below the horizon)
  creature->setNightQuery([this]() { return environment.sunDir.y < 0.0f; });
  addDynamic(creature);
  creatures.push_back(creature);
}
