#ifndef CONTROLS_MENU
#define CONTROLS_MENU

#include <string>

#include "Controls.h"
#include "SettingsMenu.h"

// Opciones > Controles: every input of the game, grouped (Movimiento,
// Camara, Acciones, Menus), and lets the player rebind the keys of the
// actions:
//   - click an action, then press the new key (Esc cancels). A key already
//     used by another action swaps with it, so no two actions share a key.
//     Esc, the mouse and the click are fixed (Controls::fixedControls).
//   - the changes are made on a copy: Guardar makes it the game's Controls
//     and writes it to the Settings file (see SettingsMenu for Por defecto /
//     Guardar / Salir and the unsaved-changes warning).
class ControlsMenu : public SettingsMenu {
private:
  Controls edited;    // what the player is changing, applied on save
  int capturing = -1; // action waiting for its new key, or -1

  void startCapture(Action action);
  void assign(Action action, int key);

protected:
  bool hasUnsavedChanges() const override { return edited != context.controls; }
  bool apply() override;
  void discard() override {} // the copy is simply dropped
  void resetToDefaults() override;
  std::string hint() const override;

public:
  explicit ControlsMenu(const MenuContext &context);

  bool onKey(int key) override; // while capturing, the key is the new one
};

#endif
