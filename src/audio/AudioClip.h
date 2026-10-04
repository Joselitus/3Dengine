#ifndef AUDIO_CLIP
#define AUDIO_CLIP

#include <string>
#include <vector>

// Sound held in memory: 32-bit float samples, interleaved by channel.
// Produced by a SpeechSynthesizer or loaded from a WAV, played by the
// SoundEngine (a clip can be played many times, by several sounds at once).
struct AudioClip {
  std::vector<float> samples;
  unsigned int channels = 1;
  unsigned int sampleRate = 22050;

  size_t frames() const { return channels ? samples.size() / channels : 0; }
  double duration() const { return sampleRate ? (double)frames() / sampleRate : 0.0; }

  // Parses a whole PCM WAV file held in memory (8/16/32-bit integer or
  // 32-bit float). False if it isn't one.
  bool loadWav(const std::string &bytes);
  // The same, reading the file at `path`. False if it can't be read or isn't
  // a supported WAV (the clip is left empty).
  bool loadWavFile(const std::string &path);
};

#endif
