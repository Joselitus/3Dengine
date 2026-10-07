#ifndef BUZZ_SYNTH
#define BUZZ_SYNTH

#include <atomic>
#include <cstdint>

#include "AudioGenerator.h"

// The whine of an insect's wings, made in real time: each wing stroke is a
// pressure pulse, so the sound is a buzz at the wingbeat frequency with many
// harmonics (a band-limited wavetable), a little wavering of the pitch (the
// wings never keep a perfect beat), a slow throb, the hiss of the air and a
// touch of saturation. setFrequency (the wingbeat, Hz: higher when the insect
// works harder) and setLevel (0..1) may be called from the game thread while
// it plays; both glide to their new values.
class BuzzSynth : public AudioGenerator {
public:
  static constexpr unsigned int RATE = 44100;
  static const int TABLE_SIZE = 4096;

private:
  std::atomic<float> targetFrequency{180.0f};
  std::atomic<float> targetLevel{0.0f};

  // audio thread state
  float table[TABLE_SIZE]; // one wing stroke (made once, band-limited for the highest pitch)
  float frequency = 180.0f, level = 0.0f;
  float phase = 0.0f, throbPhase = 0.0f, wander = 0.0f, wanderTarget = 0.0f;
  int wanderCountdown = 0;
  float noiseState = 0.0f;
  uint32_t seed = 22222u;
  float random11();

public:
  BuzzSynth();
  unsigned int sampleRate() const override { return RATE; }
  void generate(float *out, size_t frames) override;

  void setFrequency(float hz) { targetFrequency = hz; }
  void setLevel(float level) { targetLevel = level; }
};

#endif
