#ifndef PAUSE_MENU
#define PAUSE_MENU

#include "MenuContext.h"
#include "UIPanel.h"

// The menu Esc opens during the game (a UIManager key binding):
//   Reanudar   -> closes it and the game resumes (same as Esc)
//   Opciones   -> replaces it with the OptionsMenu
//   Salir      -> quits; so does the Quit key (X by default, see Controls)
// Esc closes it and the game resumes (UIManager's default for panels).
class PauseMenu : public UIPanel {
private:
  MenuContext context;

public:
  explicit PauseMenu(const MenuContext &context);

  bool dimsBackground() const override { return true; }
  bool onKey(int key) override;
};

#endif
