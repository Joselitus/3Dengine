#ifndef SOUND_ENGINE
#define SOUND_ENGINE

#include <memory>

#include <glm/glm.hpp>

#include "AudioClip.h"
#include "AudioGenerator.h"

struct ma_engine;

class SoundEngine;

// One playback of an AudioClip, returned by SoundEngine::play. It plays on
// its own (mixed by the engine's audio thread) and stops when destroyed, so
// keep it while it should sound. Spatial sounds are heard from `position`,
// fading with the distance to the listener and panned left/right.
class Sound {
private:
  struct Playback; // miniaudio's buffer and sound (SoundEngine.cpp)
  std::shared_ptr<const AudioClip> clip; // the samples must outlive playback
  std::shared_ptr<AudioGenerator> generator; // or the sound being made as it plays
  std::unique_ptr<Playback> playback;

  friend class SoundEngine;
  Sound();

public:
  Sound(const Sound &) = delete;
  Sound &operator=(const Sound &) = delete;
  ~Sound();

  bool isPlaying() const;
  double getCursorSeconds() const;  // how far it has played
  double getLengthSeconds() const;
  void setPosition(const glm::vec3 &position);
  void setVolume(float volume); // 1 = as recorded
  // Playback speed: 1 = as recorded, 2 = twice as fast and an octave higher
  void setPitch(float pitch);
  // Starts again from the beginning each time it ends, until stopped
  void setLooping(bool looping);
  // (Spatial) full volume within `nearest`, and it fades no more past `farthest` (world units;
  // by default 1.5 and 40: a small thing)
  void setDistances(float nearest, float farthest);
  void stop();
};

// The game's audio output (miniaudio: PulseAudio, ALSA... chosen at
// runtime). One per game, created after the window and destroyed after
// every Sound. If there is no audio device it still works silently:
// isAvailable() is false and play() returns nullptr.
//
// The listener (the player's ears) follows the camera: call setListener
// every frame.
//
// Each sound belongs to a Channel, with its own volume (the Audio screen):
// the music, or the game's sounds (everything else: the engine, voices,
// creatures, the wind...).
class SoundEngine {
public:
  enum class Channel { Game, Music };

private:
  struct Groups; // miniaudio's sound group of each Channel (SoundEngine.cpp)
  ma_engine *engine = nullptr;
  std::unique_ptr<Groups> groups;
  float masterVolume = 1.0f;
  float channelVolume[2] = {1.0f, 1.0f};

public:
  SoundEngine();
  SoundEngine(const SoundEngine &) = delete;
  SoundEngine &operator=(const SoundEngine &) = delete;
  ~SoundEngine();

  bool isAvailable() const { return engine != nullptr; }

  void setListener(const glm::vec3 &position, const glm::vec3 &forward);
  // The volume of everything that sounds (music, voices): 1 = as recorded
  void setMasterVolume(float volume);
  float getMasterVolume() const { return masterVolume; }
  // The volume of one Channel (1 = as recorded), on top of the master volume
  void setVolume(Channel channel, float volume);
  float getVolume(Channel channel) const { return channelVolume[(int)channel]; }

  // Starts playing `clip`. Spatial: heard from `position` in the world (mono
  // clips); otherwise straight to both ears (music, interface). `loop`: it
  // plays again and again until the Sound is stopped or destroyed.
  std::unique_ptr<Sound> play(std::shared_ptr<const AudioClip> clip,
                              bool spatial = false,
                              const glm::vec3 &position = glm::vec3(0.0f),
                              bool loop = false, Channel channel = Channel::Game);
  // Plays a sound made on the fly (see AudioGenerator), until the Sound is destroyed
  std::unique_ptr<Sound> playGenerated(std::shared_ptr<AudioGenerator> generator,
                                       bool spatial = false,
                                       const glm::vec3 &position = glm::vec3(0.0f),
                                       Channel channel = Channel::Game);
};

#endif
