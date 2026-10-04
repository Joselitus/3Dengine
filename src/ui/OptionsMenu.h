#ifndef OPTIONS_MENU
#define OPTIONS_MENU

#include "MenuContext.h"
#include "UIPanel.h"

// Opciones, opened from the PauseMenu: the list of settings screens.
//   Cámara     CameraMenu (sensitivity, field of view)
//   Controles  ControlsMenu (rebind the keys)
//   Audio      AudioMenu (master volume)
//   Volver     back to the PauseMenu (so does Esc)
// Each screen is a SettingsMenu: its changes are kept only when saved, and
// it warns about unsaved changes when left. A new screen inherits from
// SettingsMenu and gets a button here.
class OptionsMenu : public UIPanel {
private:
  MenuContext context;

  void back();

public:
  explicit OptionsMenu(const MenuContext &context);

  bool dimsBackground() const override { return true; }
  bool onKey(int key) override;
};

#endif
