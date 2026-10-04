#ifndef CONTROLS_MENU
#define CONTROLS_MENU

#include "MenuContext.h"
#include "UIPanel.h"

// Help screen, opened from the OptionsMenu: every input of the game, grouped
// (Movimiento, Camara, Acciones, Menus), built from the Controls the game
// actually uses (Controls' actions plus its fixedControls), so it can't get
// out of date. Information only for now; a rebinding screen would build on
// it (Controls::bind). "Volver" and Esc go back to the OptionsMenu.
class ControlsMenu : public UIPanel {
private:
  MenuContext context;

  void back();

public:
  explicit ControlsMenu(const MenuContext &context);

  bool dimsBackground() const override { return true; }
  bool onKey(int key) override;
};

#endif
