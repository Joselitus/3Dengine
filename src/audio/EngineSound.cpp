#include "EngineSound.h"

#include <algorithm>

namespace {
const float MIN_RPM = 60.0f; // below this the engine is stopped
const float MAX_VOLUME = 2.0f;
const int STOPPED_FRAMES = 30; // frames of silence before the Sound is dropped

float smoothstep(float a, float b, float x) {
  float t = std::min(std::max((x - a) / (b - a), 0.0f), 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
}

EngineSound::EngineSound(SoundEngine &e) : engine(e), synth(new EngineSynth()) {}

void EngineSound::update(const glm::vec3 &position, float rpm, float load, float starter,
                         float fire) {
  if (rpm < MIN_RPM && starter < 0.01f) {
    // stopped: it keeps sounding a moment so that the solenoid's release is heard
    synth->setRpm(0.0f);
    synth->setStarter(0.0f);
    synth->setFire(0.0f);
    if (++stoppedFrames > STOPPED_FRAMES)
      sound.reset();
    return;
  }
  stoppedFrames = 0;
  synth->setRpm(rpm);
  synth->setLoad(load);
  synth->setStarter(starter);
  synth->setFire(fire);
  if (!sound)
    sound = engine.playGenerated(synth, true, position);
  if (sound) {
    sound->setPosition(position);
    // louder with the revs and the load; spinning up or down it fades with them
    float level = 0.7f + 0.2f * smoothstep(600.0f, 4200.0f, rpm) + 0.15f * load;
    sound->setVolume(level * MAX_VOLUME * std::max(smoothstep(MIN_RPM, 400.0f, rpm), starter));
  }
}
