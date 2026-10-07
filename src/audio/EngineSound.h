#ifndef ENGINE_SOUND
#define ENGINE_SOUND

#include <memory>

#include <glm/glm.hpp>

#include "EngineSynth.h"
#include "SoundEngine.h"

// The sound of the engine of a vehicle: an EngineSynth (a V8 made in real time
// from the revolutions per minute and the load, see EngineSimulator) played as
// a spatial sound heard from `position`, with the volume following the revs
// and the load. Call update() every frame: when the engine is not running (rpm
// 0) it keeps no Sound. Without audio it does nothing.
class EngineSound {
private:
  SoundEngine &engine;
  std::shared_ptr<EngineSynth> synth;
  std::unique_ptr<Sound> sound;
  int stoppedFrames = 0;

public:
  explicit EngineSound(SoundEngine &engine);

  // starter: how engaged the starter motor is (0..1); fire: the share of firings that
  // happen (1 = running, 0 = cranking without firing)
  void update(const glm::vec3 &position, float rpm, float load, float starter = 0.0f,
              float fire = 1.0f);
  bool isPlaying() const { return sound != nullptr; }
};

#endif
