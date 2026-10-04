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
// PauseMenu. The values live in the Camera, so they last until the game
// closes (they are not saved to disk).
class OptionsMenu : public UIPanel {
private:
  MenuContext context;

  void back();

public:
  static constexpr float MIN_SENSITIVITY = 0.2f; // times the default
  static constexpr float MAX_SENSITIVITY = 3.0f;
  static constexpr float MIN_FOV = 40.0f; // degrees
  static constexpr float MAX_FOV = 110.0f;

  explicit OptionsMenu(const MenuContext &context);

  bool dimsBackground() const override { return true; }
  bool onKey(int key) override;
};

#endif
