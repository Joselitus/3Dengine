#ifndef OPTIONS_MENU
#define OPTIONS_MENU

#include "MenuContext.h"
#include "UIPanel.h"

// Game options, opened from the PauseMenu. Changes apply at once:
//   Sensibilidad  mouse sensitivity, as a multiple of the default
//                 (Camera::setSensitivity, SENSIVILITY = 1x)
//   FOV           vertical field of view in degrees (Camera::setFov)
//   Controles     opens the ControlsMenu (every key and what it does)
// "Restablecer" restores the defaults; "Volver" and Esc go back to the
// PauseMenu.
//
// The values live in the Camera and are kept between sessions in the
// Settings file: the menu saves them when it closes (however it is left),
// and the game loads them at start with applySettings(). A new option needs
// its key, its limits, and a line in applySettings and storeSettings.
class OptionsMenu : public UIPanel {
private:
  MenuContext context;

  void back();

public:
  static constexpr float MIN_SENSITIVITY = 0.2f; // times the default
  static constexpr float MAX_SENSITIVITY = 3.0f;
  static constexpr float MIN_FOV = 40.0f; // degrees
  static constexpr float MAX_FOV = 110.0f;

  // Keys in the Settings file
  static constexpr const char *SENSITIVITY_KEY = "camera.sensitivity";
  static constexpr const char *FOV_KEY = "camera.fov";

  // Saved values -> camera (clamped to the limits above; missing or bad
  // values keep the defaults)
  static void applySettings(const Settings &settings, Camera &camera);
  // Camera -> settings (not written to disk: see Settings::save)
  static void storeSettings(Settings &settings, const Camera &camera);

  explicit OptionsMenu(const MenuContext &context);
  // Saves the options (the menu is closing)
  ~OptionsMenu();

  bool dimsBackground() const override { return true; }
  bool onKey(int key) override;
};

#endif
