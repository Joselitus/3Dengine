#include "ControlsMenu.h"

#include <cstring>

#include "Controls.h"
#include "OptionsMenu.h"
#include "UIButton.h"
#include "UIInfoRow.h"
#include "UILabel.h"
#include "UIManager.h"

using namespace std;

// Groups in the order they are listed
static const char *const GROUPS[] = {"Movimiento", "Camara", "Acciones",
                                     "Menus"};

ControlsMenu::ControlsMenu(const MenuContext &context)
    : UIPanel("Controles", 500.0f, false), context(context) {
  const Controls &controls = context.controls;
  for (const char *group : GROUPS) {
    add(new UILabel(group, UITheme::MUTED));
    for (int i = 0; i < (int)Action::Count; i++) {
      Action action = (Action)i;
      if (strcmp(Controls::group(action), group) != 0)
        continue;
      // The key is read every frame: a rebinding would show at once
      add(new UIInfoRow(Controls::describe(action), [&controls, action]() {
        return controls.keyName(action);
      }));
    }
    for (const Controls::Fixed &fixed : Controls::fixedControls())
      if (strcmp(fixed.group, group) == 0)
        add(new UIInfoRow(fixed.description, fixed.input));
  }
  add(new UILabel("Solo informativo: aun no se pueden cambiar",
                  UITheme::MUTED));
  add(new UIButton("Volver", [this]() { back(); }));
}

void ControlsMenu::back() {
  context.ui.open(new OptionsMenu(context));
  requestClose();
}

bool ControlsMenu::onKey(int key) {
  if (key != GLFW_KEY_ESCAPE)
    return false;
  back();
  return true;
}
