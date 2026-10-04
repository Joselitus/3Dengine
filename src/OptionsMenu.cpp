#include "OptionsMenu.h"

#include "CameraMenu.h"
#include "ControlsMenu.h"
#include "PauseMenu.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UIManager.h"

using namespace std;

OptionsMenu::OptionsMenu(const MenuContext &context)
    : UIPanel("Opciones", 280.0f, false), context(context) {
  add(new UIButton("Cámara", [this]() {
    this->context.ui.open(new CameraMenu(this->context));
    requestClose();
  }));
  add(new UIButton("Controles", [this]() {
    this->context.ui.open(new ControlsMenu(this->context));
    requestClose();
  }));
  add(new UIButton("Volver", [this]() { back(); }));
  add(new UILabel("Esc: volver", UITheme::MUTED));
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
