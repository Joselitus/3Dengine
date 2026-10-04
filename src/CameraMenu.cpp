#include "CameraMenu.h"

#include "Camera.h"
#include "Settings.h"
#include "UISlider.h"

using namespace std;

void CameraMenu::applySettings(const Settings &settings, Camera &camera) {
  float sensitivity = settings.getFloat(SENSITIVITY_KEY, 1.0f);
  float fov = settings.getFloat(FOV_KEY, DEFAULT_FOV);
  camera.setSensitivity(
      glm::clamp(sensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY) * SENSIVILITY);
  camera.setFov(glm::clamp(fov, MIN_FOV, MAX_FOV));
}

void CameraMenu::storeSettings(Settings &settings, const Camera &camera) {
  settings.setFloat(SENSITIVITY_KEY, camera.getSensitivity() / SENSIVILITY);
  settings.setFloat(FOV_KEY, camera.getFov());
}

CameraMenu::CameraMenu(const MenuContext &context)
    : SettingsMenu("Cámara", 440.0f, context),
      savedSensitivity(context.camera.getSensitivity()),
      savedFov(context.camera.getFov()) {
  Camera &camera = context.camera;
  add(new UISlider(
      "Sensibilidad", MIN_SENSITIVITY, MAX_SENSITIVITY, 0.1f,
      [&camera]() { return camera.getSensitivity() / SENSIVILITY; },
      [&camera](float v) { camera.setSensitivity(v * SENSIVILITY); }, "x"));
  add(new UISlider(
      "Campo de visión (FOV)", MIN_FOV, MAX_FOV, 1.0f,
      [&camera]() { return camera.getFov(); },
      [&camera](float v) { camera.setFov(v); }, "deg"));
  addFooter();
}

bool CameraMenu::hasUnsavedChanges() const {
  return context.camera.getSensitivity() != savedSensitivity ||
         context.camera.getFov() != savedFov;
}

bool CameraMenu::apply() {
  storeSettings(context.settings, context.camera);
  if (!context.settings.save())
    return false;
  savedSensitivity = context.camera.getSensitivity();
  savedFov = context.camera.getFov();
  return true;
}

void CameraMenu::discard() {
  context.camera.setSensitivity(savedSensitivity);
  context.camera.setFov(savedFov);
}

void CameraMenu::resetToDefaults() {
  context.camera.setSensitivity(SENSIVILITY);
  context.camera.setFov(DEFAULT_FOV);
}

string CameraMenu::hint() const {
  return "Los cambios se ven al momento.";
}
