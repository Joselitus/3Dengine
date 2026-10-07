#include "MusicPlayer.h"

using namespace std;

void MusicPlayer::play(shared_ptr<const AudioClip> music, bool loop,
                       float volume) {
  sound.reset(); // stops the old one
  clip = music;
  if (!music)
    return;
  sound = engine.play(music, false, glm::vec3(0.0f), loop, channel);
  if (sound)
    sound->setVolume(volume);
}
