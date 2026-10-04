#ifndef ESPEAK_SYNTHESIZER
#define ESPEAK_SYNTHESIZER

#include <string>

#include "SpeechSynthesizer.h"

// SpeechSynthesizer using the espeak-ng program (must be installed; Linux).
// Each call runs `espeak-ng -v <language> -s <wpm> -p <pitch> --stdin
// --stdout`, writes the text to its input (no shell involved, so any text is
// safe) and reads the WAV it prints. Mono, 22050 Hz.
class EspeakSynthesizer : public SpeechSynthesizer {
private:
  std::string program;

public:
  explicit EspeakSynthesizer(const std::string &program = "espeak-ng")
      : program(program) {}

  std::shared_ptr<AudioClip> synthesize(const std::string &text,
                                        const VoiceSettings &voice) override;
};

#endif
