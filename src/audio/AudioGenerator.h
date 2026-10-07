#ifndef AUDIO_GENERATOR
#define AUDIO_GENERATOR

#include <cstddef>

// A sound made sample by sample while it plays (an engine...), instead of
// read from an AudioClip. SoundEngine::playGenerated calls generate() from
// the audio thread: it must not block or allocate, and anything the game
// changes while it plays (an engine's speed) has to be atomic.
class AudioGenerator {
public:
  virtual ~AudioGenerator() {}
  virtual unsigned int sampleRate() const = 0;
  // Writes `frames` mono samples (32-bit float)
  virtual void generate(float *out, size_t frames) = 0;
};

#endif
