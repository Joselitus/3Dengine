#include "OptionsMenu.h"

#include "Camera.h"
#include "ControlsMenu.h"
#include "PauseMenu.h"
#include "Settings.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UIManager.h"
#include "UIRow.h"
#include "UISlider.h"

using namespace std;

OptionsMenu::OptionsMenu(const MenuContext &context)
    : UIPanel("Opciones", 360.0f, false), context(context) {
  Camera &camera = context.camera;
  add(new UISlider(
      "Sensibilidad", MIN_SENSITIVITY, MAX_SENSITIVITY, 0.1f,
      [&camera]() { return camera.getSensitivity() / SENSIVILITY; },
      [&camera](float v) { camera.setSensitivity(v * SENSIVILITY); }, "x"));
  add(new UISlider(
      "Campo de vision (FOV)", MIN_FOV, MAX_FOV, 1.0f,
      [&camera]() { return camera.getFov(); },
      [&camera](float v) { camera.setFov(v); }, "deg"));

  add(new UIButton("Controles", [this]() {
    this->context.ui.open(new ControlsMenu(this->context));
    requestClose();
  }));

  UIRow *buttons = add(new UIRow());
  buttons->add(new UIButton("Restablecer", [&camera]() {
    camera.setSensitivity(SENSIVILITY);
    camera.setFov(DEFAULT_FOV);
  }));
  buttons->add(new UIButton("Volver", [this]() { back(); }));
  add(new UILabel("Esc: volver", UITheme::MUTED));
}

void OptionsMenu::applySettings(const Settings &settings, Camera &camera) {
  float sensitivity = settings.getFloat(SENSITIVITY_KEY, 1.0f);
  float fov = settings.getFloat(FOV_KEY, DEFAULT_FOV);
  camera.setSensitivity(
      glm::clamp(sensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY) * SENSIVILITY);
  camera.setFov(glm::clamp(fov, MIN_FOV, MAX_FOV));
}

void OptionsMenu::storeSettings(Settings &settings, const Camera &camera) {
  settings.setFloat(SENSITIVITY_KEY, camera.getSensitivity() / SENSIVILITY);
  settings.setFloat(FOV_KEY, camera.getFov());
}

OptionsMenu::~OptionsMenu() {
  storeSettings(context.settings, context.camera);
  context.settings.save();
}

void OptionsMenu::back() {
  context.ui.open(new PauseMenu(context));
  requestClose();
}

bool OptionsMenu::onKey(int key) {
  if (key != GLFW_KEY_ESCAPE)
    return false;
  back();
  return true;
}
