#include "SceneStage.h"

#include "Walker.h"

using namespace std;
using namespace glm;

// Gravity of the player, units / second^2 (same as in TestStage)
#define PLAYER_GRAVITY 25.0f

unique_ptr<SceneStage> SceneStage::load(const string &path,
                                        const string &assetDir,
                                        FloorMode mode) {
  SceneFile file;
  if (!file.load(path))
    return nullptr;
  return unique_ptr<SceneStage>(new SceneStage(file, assetDir, mode));
}

constexpr unsigned int SceneStage::PENGUIN_ANIMATION;

SceneStage::SceneStage(const SceneFile &file, const string &assetDir,
                       FloorMode mode)
    : GameStage(mode) {
  const string prefix = assetDir + "/";

  environment.lightDir = file.moonDir;
  environment.lightColor = file.lightColor;
  environment.horizon = file.fogColor;
  setDayDuration(file.dayDuration);
  setTimeOfDay(file.timeOfDay);
  walkCameraDistance = file.cameraDistance;
  walkCameraHeight = file.cameraHeight;
  if (!file.sky.empty())
    setSky(loadModel(prefix + file.sky));

  // The floor first, so the rest can stand on it
  const vec3 &f = file.floorPosition;
  if (!file.floor.empty()) {
    auto floor = make_shared<GameObject>(loadModel(prefix + file.floor));
    floor->setPosition(f.x, f.y, f.z);
    add(floor);
    // (a .scene has no material map: its floor is all sand)
    setFloor(loadModel(prefix + file.floor), f,
             MaterialMap::uniform(FloorMaterial::Sand));
  }

  for (const SceneObject &o : file.objects) {
    auto object = make_shared<GameObject>();
    object->addPart(loadModel(prefix + o.model),
                    o.effect == Effect::Emissive ? 2 : 0);
    float y = o.onGround ? groundAt(o.position.x, o.position.z, f.y) - PROP_SINK
                         : o.position.y;
    object->setPosition(o.position.x, y, o.position.z);
    object->setYaw(o.yaw);
    object->setScale(o.scale);
    if (o.effect == Effect::Breathe)
      object->setBreathAmp(BREATH_AMPLITUDE);
    if (o.effect == Effect::Sway)
      object->setSwayAmp(1.0f);
    add(object);
  }

  // The players appear where the file says (without a player in it, at the origin), as the
  // penguin or as the model the file names
  const vec3 &p = file.playerPosition;
  spawnPoint = vec3(p.x, file.playerOnGround ? groundAt(p.x, p.z, f.y) : p.y, p.z);
  if (!file.player.empty())
    avatarModel = prefix + file.player;
}
