#ifndef VOICE
#define VOICE

#include <future>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "LineNarrator.h"
#include "SoundEngine.h"
#include "SpeechSynthesizer.h"

// Something that speaks (the LineNarrator of NPCs): says one text at a
// time, out loud, from a position in the world. say() returns at once: the text is synthesized on another
// thread, then played through the SoundEngine as a spatial sound.
//
// progress() tells how much of the text has been spoken (0..1), so subtitles
// can show it as it is read. Without audio (no device, or the synthesizer
// failed) the voice still "reads" at CHARS_PER_SECOND, so subtitles work the
// same, silently. Call update() every frame.
class Voice : public LineNarrator {
public:
  enum class State { Idle, Synthesizing, Speaking };

private:
  SoundEngine &engine;
  SpeechSynthesizer &synthesizer;
  VoiceSettings settings;

  State state = State::Idle;
  std::string text;
  std::future<std::shared_ptr<AudioClip>> pending;
  std::unique_ptr<Sound> sound;
  double silentTime = -1.0; // >= 0 while reading without audio
  double silentLength = 0.0;
  glm::vec3 position = glm::vec3(0.0f);

public:
  static constexpr double CHARS_PER_SECOND = 14.0; // reading speed, silent

  Voice(SoundEngine &engine, SpeechSynthesizer &synthesizer,
        const VoiceSettings &settings = VoiceSettings());
  // Waits for a synthesis still running (it can't be cancelled)
  ~Voice();

  // Starts saying `text` (UTF-8), interrupting what it was saying
  void say(const std::string &text) override;
  void stop() override;
  // Advances the speech; `position`: where the voice comes from now
  void update(double dt, const glm::vec3 &position) override;

  State getState() const { return state; }
  bool isSpeaking() const override { return state != State::Idle; }
  bool isPreparing() const override { return state == State::Synthesizing; }
  const std::string &getText() const { return text; }
  // How much of the text has been said: 0 before it starts, 1 when done
  float progress() const override;
};

#endif
