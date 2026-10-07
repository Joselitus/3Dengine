#ifndef ENGINE_SOUND
#define ENGINE_SOUND

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "EngineSynth.h"
#include "SoundEngine.h"

// The sound of the engine of a vehicle: an EngineSynth (a V8 made in real time
// from the revolutions per minute and the load, see EngineSimulator) played as
// a spatial sound heard from `position`, with the volume following the revs
// and the load. Call update() every frame: when the engine is not running (rpm
// 0) it keeps no Sound. Without audio it does nothing.
//
// With setStartClip, the starter cranking the engine is that recording (played
// once per attempt) instead of the synthesized one; the synth is only the
// running engine.
class EngineSound {
private:
  SoundEngine &engine;
  std::shared_ptr<EngineSynth> synth;
  std::unique_ptr<Sound> sound;
  std::shared_ptr<AudioClip> startClip;
  std::unique_ptr<Sound> startSound;
  bool wasCranking = false;
  int stoppedFrames = 0;

public:
  explicit EngineSound(SoundEngine &engine);

  // Loads the recorded sound of the starter (a WAV). False if it can't be read.
  bool setStartClip(const std::string &path);
  double getStartClipLength() const { return startClip ? startClip->duration() : 0.0; }

  // cranking: the starter is turning the engine (EngineSimulator::Phase::Cranking).
  // starter: how engaged the starter motor is (0..1); fire: the share of firings that
  // happen (1 = running, 0 = cranking without firing)
  void update(const glm::vec3 &position, float rpm, float load, float starter = 0.0f,
              float fire = 1.0f, bool cranking = false);
  bool isPlaying() const { return sound != nullptr; }
};

#endif
