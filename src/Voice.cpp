#include "Voice.h"

#include <algorithm>
#include <chrono>

using namespace std;

Voice::Voice(SoundEngine &engine, SpeechSynthesizer &synthesizer,
             const VoiceSettings &settings)
    : engine(engine), synthesizer(synthesizer), settings(settings) {}

Voice::~Voice() {
  if (pending.valid())
    pending.wait();
}

void Voice::say(const string &newText) {
  stop();
  text = newText;
  state = State::Synthesizing;
  // Copies, not references: the thread may outlive this call
  SpeechSynthesizer *tts = &synthesizer;
  VoiceSettings voice = settings;
  pending = async(launch::async, [tts, newText, voice]() {
    return tts->synthesize(newText, voice);
  });
}

void Voice::stop() {
  if (sound)
    sound->stop();
  sound.reset();
  silentTime = -1.0;
  state = State::Idle;
  // A synthesis still running is left to finish; its result is ignored
  // (the next say() replaces `pending`, which waits for it)
}

void Voice::update(double dt, const glm::vec3 &where) {
  position = where;
  if (state == State::Synthesizing && pending.valid() &&
      pending.wait_for(chrono::seconds(0)) == future_status::ready) {
    shared_ptr<AudioClip> clip = pending.get();
    sound = engine.play(clip, true, position);
    if (sound) {
      state = State::Speaking;
    } else {
      // No audio: read it silently, at the speed of the clip if there is
      // one, otherwise at CHARS_PER_SECOND
      silentLength = clip ? clip->duration() : text.size() / CHARS_PER_SECOND;
      silentTime = 0.0;
      state = State::Speaking;
    }
  }

  if (state == State::Speaking) {
    if (sound) {
      sound->setPosition(position);
      if (!sound->isPlaying())
        state = State::Idle;
    } else if (silentTime >= 0.0) {
      silentTime += dt;
      if (silentTime >= silentLength)
        state = State::Idle;
    }
  }
}

float Voice::progress() const {
  switch (state) {
  case State::Synthesizing:
    return 0.0f;
  case State::Speaking: {
    double length = sound ? sound->getLengthSeconds() : silentLength;
    double done = sound ? sound->getCursorSeconds() : silentTime;
    return length > 0.0 ? (float)min(1.0, done / length) : 1.0f;
  }
  case State::Idle:
  default:
    return text.empty() ? 0.0f : 1.0f;
  }
}
