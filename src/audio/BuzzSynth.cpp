#include "BuzzSynth.h"

#include <cmath>

namespace {
const float PI = 3.14159265f;
const int HARMONICS = 40;           // the table holds this many (for wingbeats up to ~500 Hz)
const float WANDER = 0.035f;        // how far the pitch wavers (fraction)
const float THROB_HZ = 3.1f;        // the slow pulsing of the buzz
const float THROB_DEPTH = 0.18f;
const float NOISE = 0.10f;          // the air's hiss, relative to the buzz
}

BuzzSynth::BuzzSynth() {
  // One stroke: the harmonics fall off slowly and those around 4-8 times the
  // wingbeat are lifted (the nasal whine of a mosquito); their phases are fixed
  // but scattered, so the pulse is not a plain sawtooth
  float peak = 0.0f;
  for (int i = 0; i < TABLE_SIZE; i++) {
    float x = 2.0f * PI * i / TABLE_SIZE;
    float v = 0.0f;
    for (int k = 1; k <= HARMONICS; k++) {
      float amp = 1.0f / std::pow((float)k, 0.85f);
      amp *= 1.0f + 1.6f * std::exp(-std::pow((k - 6.0f) / 2.5f, 2.0f));
      v += amp * std::sin(k * x + 1.7f * k * k);
    }
    table[i] = v;
    peak = std::fmax(peak, std::fabs(v));
  }
  for (int i = 0; i < TABLE_SIZE; i++)
    table[i] /= peak;
}

float BuzzSynth::random11() {
  seed = seed * 1664525u + 1013904223u;
  return (float)(seed >> 8) / 8388608.0f - 1.0f;
}

void BuzzSynth::generate(float *out, size_t frames) {
  float wantFrequency = targetFrequency.load(), wantLevel = targetLevel.load();
  const float glide = 1.0f - std::exp(-1.0f / (0.08f * RATE)); // ~80 ms
  for (size_t n = 0; n < frames; n++) {
    frequency += (wantFrequency - frequency) * glide;
    level += (wantLevel - level) * glide;
    // the pitch wavers: a new random aim every ~40 ms, followed smoothly
    if (--wanderCountdown <= 0) {
      wanderTarget = WANDER * random11();
      wanderCountdown = RATE / 25;
    }
    wander += (wanderTarget - wander) * 0.0004f;
    throbPhase += THROB_HZ / RATE;
    if (throbPhase >= 1.0f)
      throbPhase -= 1.0f;
    float f = frequency * (1.0f + wander);
    phase += f / RATE;
    if (phase >= 1.0f)
      phase -= 1.0f;
    float position = phase * TABLE_SIZE;
    int i = (int)position;
    float t = position - i;
    float buzz = table[i] + (table[(i + 1) % TABLE_SIZE] - table[i]) * t;
    buzz *= 1.0f - THROB_DEPTH * (0.5f + 0.5f * std::sin(2.0f * PI * throbPhase));
    // the hiss: white noise, a little low-passed, louder at each stroke
    noiseState += (random11() - noiseState) * 0.35f;
    float hiss = NOISE * noiseState * (0.6f + 0.4f * std::fabs(buzz));
    out[n] = level * std::tanh(1.4f * (buzz + hiss)) * 0.8f;
  }
}
