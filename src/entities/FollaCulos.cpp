#include "FollaCulos.h"

#include <cmath>

using namespace std;
using namespace glm;

FollaCulos::FollaCulos(shared_ptr<AnimatedModel> model, SoundEngine &engine,
                       SpeechSynthesizer &synthesizer)
    : Npc(model, "Folla Culos", vector<string>(), engine, synthesizer) {
  model->useRealSize();
  setDrag(0.0f);               // it runs at its own pace (no NPC drag)
  setMaxSpeed(RUN_SPEED);
  setMaxAcceleration(60.0f);
}

void FollaCulos::update(double dt) {
  // A straight line towards the target, on the ground plane
  vec3 wanted(0.0f);
  running = false;
  if (targetPosition) {
    vec3 d = targetPosition() - position;
    d.y = 0.0f;
    float distance = length(d);
    bool night = !isNight || isNight();
    // At night it goes for the target; by day it goes away from it
    bool goes = night ? distance > STOP_DISTANCE : distance < FLEE_DISTANCE;
    if (goes && distance > 1e-3f) {
      vec3 direction = night ? d / distance : -d / distance;
      wanted = direction * RUN_SPEED;
      faceTowards(position + direction); // yaw 0 is towards +z, like the Walker
      running = true;
    }
  }
  wanted.y = velocity.y; // (falling is not steered)
  steerTowards(wanted, 12.0f);
  DynamicGameObject::update(dt);
  // The animation: only while it runs (it stands still in the pose it stopped in)
  if (running)
    runClock += dt;
  if (aniModel)
    aniModel->Update(runClock);
}

void FollaCulos::getLight(vector<SpotLight> &lights) const {
  if (!visible)
    return;
  vec3 forward = vec3(rotation * vec4(0.0f, 0.0f, 1.0f, 0.0f));
  lights.push_back(SpotLight::omni(position + vec3(0.0f, EYE_HEIGHT, 0.0f) + forward * 0.5f,
                                   vec3(0.55f, 0.42f, 0.02f), EYE_LIGHT_RANGE));
}

void FollaCulos::describe(vector<string> &lines) const {
  Npc::describe(lines);
  lines.push_back(string("Corriendo: ") + (running ? "si" : "no") +
                  ((!isNight || isNight()) ? "  (de noche: hacia el jugador)" : "  (de dia: huye)"));
}
