#ifndef SETTINGS_MENU
#define SETTINGS_MENU

#include <string>

#include "MenuContext.h"
#include "UIPanel.h"

// Base of the settings screens inside Opciones (CameraMenu, ControlsMenu):
// they edit something and only keep it when the player saves.
//
// A subclass adds its controls in its constructor and ends it with
// addFooter(), which adds a status text and the buttons:
//   Por defecto  resetToDefaults() (still to be saved)
//   Guardar      apply(): make the changes the ones in use and write the
//                Settings file
//   Volver       back to the OptionsMenu. With unsaved changes it first asks
//                with a ConfirmDialog: "Guardar y salir" or "Salir", which
//                discard()s them. Esc on that dialog returns to the screen.
// Esc does the same as Volver.
class SettingsMenu : public UIPanel {
protected:
  MenuContext context;
  std::string status; // last thing that happened, shown above the buttons

  SettingsMenu(const std::string &title, float width,
               const MenuContext &context);

  // Status text, 2 lines, and the Por defecto / Guardar / Volver row
  void addFooter();

  virtual bool hasUnsavedChanges() const = 0;
  // Make the edited values the ones in use and save them; false on error
  virtual bool apply() = 0;
  // Undo what wasn't saved (the screen is being left without saving)
  virtual void discard() = 0;
  virtual void resetToDefaults() = 0;
  // Shown while there is no status
  virtual std::string hint() const { return ""; }

  void save();
  void leave(); // asks first if there are unsaved changes
  void back();  // to the OptionsMenu (what isn't saved must be gone)

public:
  bool dimsBackground() const override { return true; }
  bool onKey(int key) override; // Esc: leave()
};

#endif
