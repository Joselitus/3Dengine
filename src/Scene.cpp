#include "Scene.h"

using namespace std;
using namespace glm;

// Amplitude of the breathing for objects with Effect::Breathe
#define BREATH_AMPLITUDE 2.0f

shared_ptr<Model> Scene::getModel(const string &path) {
  shared_ptr<Model> &model = models[path];
  if (!model)
    model = make_shared<Model>(path.c_str());
  return model;
}

bool Scene::load(const string &scenePath, const string &assetDir) {
  if (!file.load(scenePath))
    return false;
  const string prefix = assetDir + "/";

  if (!file.sky.empty())
    sky.reset(new GameObject(getModel(prefix + file.sky)));

  for (const SceneObject &o : file.objects) {
    GameObject object(getModel(prefix + o.model));
    object.setPosition(o.position.x, o.position.y, o.position.z);
    object.setRotation(glm::scale(glm::rotate(mat4(1.0f), o.yaw,
                                              vec3(0.0f, 1.0f, 0.0f)),
                                  vec3(o.scale)));
    objects.push_back(object);
    effects.push_back(o.effect);
  }

  if (!file.player.empty()) {
    playerModel = make_shared<AnimatedModel>((prefix + file.player).c_str());
    player.reset(new GameObject(playerModel));
    const vec3 &p = file.playerPosition;
    player->setPosition(p.x, p.y, p.z);
  }
  return true;
}

void Scene::Update(double seconds) {
  if (playerModel)
    playerModel->Update(seconds);
}

void Scene::Draw(Shader *shader, vec3 cameraPosition, float time) {
  shader->setVector3("moonDir", file.moonDir.x, file.moonDir.y,
                     file.moonDir.z);
  shader->setVector3("fogColor", file.fogColor.r, file.fogColor.g,
                     file.fogColor.b);
  shader->setFloat("time", time);
  shader->setFloat("breathTime", time);
  shader->setFloat("breathAmp", 0.0f);

  // The sky is centred on the camera and drawn first, without depth, so
  // everything else ends up in front of it.
  if (sky) {
    glDisable(GL_DEPTH_TEST);
    shader->setInt("unlit", 1);
    sky->setPosition(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    sky->Draw(shader);
    glEnable(GL_DEPTH_TEST);
  }

  for (size_t i = 0; i < objects.size(); i++) {
    shader->setInt("unlit", effects[i] == Effect::Emissive ? 2 : 0);
    shader->setFloat("breathAmp",
                     effects[i] == Effect::Breathe ? BREATH_AMPLITUDE : 0.0f);
    objects[i].Draw(shader);
  }
  shader->setInt("unlit", 0);
  shader->setFloat("breathAmp", 0.0f);

  if (player)
    player->Draw(shader);
}
