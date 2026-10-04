#include "AudioMenu.h"

#include "Settings.h"
#include "SoundEngine.h"
#include "UISlider.h"

using namespace std;

void AudioMenu::applySettings(const Settings &settings, SoundEngine &sound) {
  float volume = settings.getFloat(VOLUME_KEY, DEFAULT_VOLUME);
  sound.setMasterVolume(glm::clamp(volume, MIN_VOLUME, MAX_VOLUME));
}

void AudioMenu::storeSettings(Settings &settings, const SoundEngine &sound) {
  settings.setFloat(VOLUME_KEY, sound.getMasterVolume());
}

AudioMenu::AudioMenu(const MenuContext &context)
    : SettingsMenu("Audio", 440.0f, context),
      savedVolume(context.sound.getMasterVolume()) {
  SoundEngine &sound = context.sound;
  // Shown as a percentage; the engine takes a fraction
  add(new UISlider(
      "Volumen general", MIN_VOLUME * 100.0f, MAX_VOLUME * 100.0f, 5.0f,
      [&sound]() { return sound.getMasterVolume() * 100.0f; },
      [&sound](float v) { sound.setMasterVolume(v / 100.0f); }, "%"));
  addFooter();
}

bool AudioMenu::hasUnsavedChanges() const {
  return context.sound.getMasterVolume() != savedVolume;
}

bool AudioMenu::apply() {
  storeSettings(context.settings, context.sound);
  if (!context.settings.save())
    return false;
  savedVolume = context.sound.getMasterVolume();
  return true;
}

void AudioMenu::discard() { context.sound.setMasterVolume(savedVolume); }

void AudioMenu::resetToDefaults() {
  context.sound.setMasterVolume(DEFAULT_VOLUME);
}

string AudioMenu::hint() const { return "Los cambios se oyen al momento."; }
