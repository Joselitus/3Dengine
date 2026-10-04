#include "PauseMenu.h"

#include "Controls.h"
#include "OptionsMenu.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UIManager.h"

using namespace std;

PauseMenu::PauseMenu(const MenuContext &context)
    : UIPanel("Pausa", 280.0f, false), context(context) {
  add(new UIButton("Reanudar", [this]() { requestClose(); }));
  add(new UIButton("Opciones", [this]() {
    this->context.ui.open(new OptionsMenu(this->context));
    requestClose();
  }));
  add(new UIButton("Salir (" + context.controls.keyName(Action::Quit) + ")",
                   context.quit));
  add(new UILabel("Esc: volver al juego", UITheme::MUTED));
}

bool PauseMenu::onKey(int key) {
  if (key != context.controls.key(Action::Quit))
    return false;
  context.quit();
  return true;
}
