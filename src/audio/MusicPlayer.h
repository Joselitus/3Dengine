#ifndef MUSIC_PLAYER
#define MUSIC_PLAYER

#include <memory>

#include "SoundEngine.h"

// Plays the background music of the current map: one track at a time, heard
// straight in both ears (not from a place in the world). play() replaces
// whatever was playing; a null clip means silence. Create it after the
// SoundEngine (it must be destroyed before it).
class MusicPlayer {
private:
  SoundEngine &engine;
  std::shared_ptr<const AudioClip> clip; // what is playing now
  std::unique_ptr<Sound> sound;

public:
  explicit MusicPlayer(SoundEngine &engine) : engine(engine) {}

  // Starts `music` from the beginning (stopping the previous track), looping
  // if `loop`, at `volume` (1 = as recorded). nullptr stops the music.
  void play(std::shared_ptr<const AudioClip> music, bool loop = true,
            float volume = 1.0f);
  void stop() { play(nullptr); }
  bool isPlaying() const { return sound && sound->isPlaying(); }
  // Seconds into the track (0 if nothing plays); it goes back to 0 on a loop
  double getCursorSeconds() const { return sound ? sound->getCursorSeconds() : 0.0; }
};

#endif
