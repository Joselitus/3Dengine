#include "AudioMenu.h"

#include "Settings.h"
#include "SoundEngine.h"
#include "UISlider.h"

using namespace std;

typedef SoundEngine::Channel Channel;

void AudioMenu::applySettings(const Settings &settings, SoundEngine &sound) {
  float old = settings.getFloat(OLD_VOLUME_KEY, DEFAULT_VOLUME);
  float music = settings.getFloat(MUSIC_KEY, old);
  float game = settings.getFloat(GAME_KEY, old);
  sound.setVolume(Channel::Music, glm::clamp(music, MIN_VOLUME, MAX_VOLUME));
  sound.setVolume(Channel::Game, glm::clamp(game, MIN_VOLUME, MAX_VOLUME));
}

void AudioMenu::storeSettings(Settings &settings, const SoundEngine &sound) {
  settings.setFloat(MUSIC_KEY, sound.getVolume(Channel::Music));
  settings.setFloat(GAME_KEY, sound.getVolume(Channel::Game));
}

// A slider for one channel's volume: shown as a percentage, the engine takes a fraction
static UISlider *volumeSlider(const string &label, SoundEngine &sound, Channel channel) {
  return new UISlider(
      label, AudioMenu::MIN_VOLUME * 100.0f, AudioMenu::MAX_VOLUME * 100.0f, 5.0f,
      [&sound, channel]() { return sound.getVolume(channel) * 100.0f; },
      [&sound, channel](float v) { sound.setVolume(channel, v / 100.0f); }, "%");
}

AudioMenu::AudioMenu(const MenuContext &context)
    : SettingsMenu("Audio", 440.0f, context),
      savedMusic(context.sound.getVolume(Channel::Music)),
      savedGame(context.sound.getVolume(Channel::Game)) {
  add(volumeSlider("Música", context.sound, Channel::Music));
  add(volumeSlider("Sonidos del juego", context.sound, Channel::Game));
  addFooter();
}

bool AudioMenu::hasUnsavedChanges() const {
  return context.sound.getVolume(Channel::Music) != savedMusic ||
         context.sound.getVolume(Channel::Game) != savedGame;
}

bool AudioMenu::apply() {
  storeSettings(context.settings, context.sound);
  if (!context.settings.save())
    return false;
  savedMusic = context.sound.getVolume(Channel::Music);
  savedGame = context.sound.getVolume(Channel::Game);
  return true;
}

void AudioMenu::discard() {
  context.sound.setVolume(Channel::Music, savedMusic);
  context.sound.setVolume(Channel::Game, savedGame);
}

void AudioMenu::resetToDefaults() {
  context.sound.setVolume(Channel::Music, DEFAULT_VOLUME);
  context.sound.setVolume(Channel::Game, DEFAULT_VOLUME);
}

string AudioMenu::hint() const { return "Los cambios se oyen al momento."; }
