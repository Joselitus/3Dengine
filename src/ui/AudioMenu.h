#ifndef AUDIO_MENU
#define AUDIO_MENU

#include "SettingsMenu.h"

class Settings;
class SoundEngine;

// Opciones > Audio: how loud the game is.
//   Volumen general  the master volume (SoundEngine::setMasterVolume), in
//                    percent of the volume as recorded: it scales everything
//                    that sounds (the music and the voices)
// The slider changes the volume at once, so it can be heard; Guardar keeps it
// (Settings file), leaving without saving puts back the volume the engine
// had when the screen opened (see SettingsMenu for the buttons and the
// unsaved-changes warning).
//
// The game loads the saved value at start with applySettings(). A new option
// needs its key, its limits, a slider, and a line in applySettings,
// storeSettings, resetToDefaults and the saved/edited comparison.
class AudioMenu : public SettingsMenu {
private:
  float savedVolume; // master volume when the screen opened

protected:
  bool hasUnsavedChanges() const override;
  bool apply() override;
  void discard() override;
  void resetToDefaults() override;
  std::string hint() const override;

public:
  static constexpr float MIN_VOLUME = 0.0f; // fractions of the recorded volume
  static constexpr float MAX_VOLUME = 1.0f;
  static constexpr float DEFAULT_VOLUME = 1.0f;
  // Key in the Settings file
  static constexpr const char *VOLUME_KEY = "audio.master_volume";

  // Saved value -> sound engine (clamped to the limits above; a missing or bad
  // value keeps the default)
  static void applySettings(const Settings &settings, SoundEngine &sound);
  // Sound engine -> settings (not written to disk: see Settings::save)
  static void storeSettings(Settings &settings, const SoundEngine &sound);

  explicit AudioMenu(const MenuContext &context);
};

#endif
