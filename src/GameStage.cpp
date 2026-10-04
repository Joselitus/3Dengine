#include "GameStage.h"

using namespace std;
using namespace glm;

void GameStage::setSky(shared_ptr<Model> model) {
  sky = make_shared<GameObject>();
  sky->addPart(model, 1); // unlit = 1: sky dome
}

float GameStage::groundAt(float x, float z, float fallback) const {
  float height;
  return floorAt(x, z, height) ? height : fallback;
}

void GameStage::render(Shader *shader, const vec3 &cameraPosition,
                       double time) {
  if (sky) {
    shader->setFloat("time", (float)time); // star twinkle
    glDisable(GL_DEPTH_TEST);
    sky->setPosition(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    sky->Draw(shader);
    glEnable(GL_DEPTH_TEST);
  }
  Draw(shader, time);
}
