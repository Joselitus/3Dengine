#ifndef SILENT_SYNTHESIZER
#define SILENT_SYNTHESIZER

#include "SpeechSynthesizer.h"

// A speech synthesizer that says nothing: for the server, whose characters never speak (what
// they say is spoken, and heard, on each client).
class SilentSynthesizer : public SpeechSynthesizer {
public:
  std::shared_ptr<AudioClip> synthesize(const std::string &, const VoiceSettings &) override {
    return nullptr;
  }
};

#endif
