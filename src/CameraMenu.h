#ifndef CAMERA_MENU
#define CAMERA_MENU

#include "SettingsMenu.h"

class Camera;
class Settings;

// Opciones > Camara: how the view behaves.
//   Sensibilidad  mouse sensitivity, as a multiple of the default
//                 (Camera::setSensitivity, SENSIVILITY = 1x)
//   FOV           vertical field of view in degrees (Camera::setFov)
// The sliders change the camera at once, so the effect can be seen; Guardar
// keeps them (Settings file), leaving without saving puts back the values
// the camera had when the screen opened (see SettingsMenu for the buttons
// and the unsaved-changes warning).
//
// The game loads the saved values at start with applySettings(). A new
// option needs its key, its limits, a slider, and a line in applySettings,
// storeSettings, resetToDefaults and the saved/edited comparison.
class CameraMenu : public SettingsMenu {
private:
  float savedSensitivity, savedFov; // camera values when the screen opened

protected:
  bool hasUnsavedChanges() const override;
  bool apply() override;
  void discard() override;
  void resetToDefaults() override;
  std::string hint() const override;

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

  explicit CameraMenu(const MenuContext &context);
};

#endif
