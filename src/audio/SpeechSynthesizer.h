#ifndef SPEECH_SYNTHESIZER
#define SPEECH_SYNTHESIZER

#include <memory>
#include <string>

#include "AudioClip.h"

// How a voice sounds. Each synthesizer maps it to its own settings.
struct VoiceSettings {
  std::string language = "es"; // e.g. "es", "es-419", "en"
  int wordsPerMinute = 160;
  int pitch = 50; // 0..99, 50 = normal
};

// Text to speech (abstract): turns text into audio. synthesize() blocks
// until the audio is ready (it can take a moment), so the game calls it from
// another thread (see Voice). Implementations must be safe to call from
// several threads at once. Text is UTF-8: accents help the pronunciation.
//
// Current implementation: EspeakSynthesizer (espeak-ng). A better voice
// (e.g. a neural TTS) only needs another subclass.
class SpeechSynthesizer {
public:
  virtual ~SpeechSynthesizer() {}
  // nullptr on failure (the reason is printed)
  virtual std::shared_ptr<AudioClip> synthesize(const std::string &text,
                                                const VoiceSettings &voice) = 0;
};

#endif
