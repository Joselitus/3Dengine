#include "ControlsMenu.h"

#include <cstring>

#include "Settings.h"
#include "UIInfoRow.h"
#include "UILabel.h"
#include "UIManager.h"

using namespace std;

// Groups in the order they are listed
static const char *const GROUPS[] = {"Movimiento", "Acciones",
                                     "Menus"};

ControlsMenu::ControlsMenu(const MenuContext &context)
    : SettingsMenu("Controles", 500.0f, context), edited(context.controls) {
  for (const char *group : GROUPS) {
    add(new UILabel(group, UITheme::MUTED));
    for (int i = 0; i < (int)Action::Count; i++) {
      Action action = (Action)i;
      if (strcmp(Controls::group(action), group) != 0)
        continue;
      add(new UIInfoRow(
          Controls::describe(action),
          [this, i]() {
            return capturing == i ? string("Pulsa una tecla...")
                                  : edited.keyName((Action)i);
          },
          [this, action]() { startCapture(action); }));
    }
    for (const Controls::Fixed &fixed : Controls::fixedControls())
      if (strcmp(fixed.group, group) == 0)
        add(new UIInfoRow(fixed.description, fixed.input));
  }

  addFooter();
}

void ControlsMenu::startCapture(Action action) {
  capturing = (int)action;
  status = string("Pulsa la nueva tecla para '") + Controls::describe(action) +
           "' (Esc: cancelar).";
}

void ControlsMenu::assign(Action action, int key) {
  int old = edited.key(action);
  Action other = edited.actionFor(key);
  edited.bind(action, key);
  if (other != Action::Count && other != action) {
    // No two actions on one key: the other one gets the old key
    edited.bind(other, old);
    status = Controls::keyName(key) + " estaba en '" +
             Controls::describe(other) + "': se han intercambiado.";
  } else {
    status = string("'") + Controls::describe(action) + "': " +
             Controls::keyName(key) + ".";
  }
}

bool ControlsMenu::apply() {
  context.controls = edited;
  context.controls.writeTo(context.settings);
  return context.settings.save();
}

void ControlsMenu::resetToDefaults() {
  edited = Controls();
  capturing = -1;
}

string ControlsMenu::hint() const {
  return "Clic en una acción: cambia su tecla. Esc: menú / atrás.";
}

bool ControlsMenu::onKey(int key) {
  if (capturing >= 0) {
    if (key == GLFW_KEY_ESCAPE)
      status = "Cancelado.";
    else if (Controls::isBindable(key))
      assign((Action)capturing, key);
    capturing = -1;
    return true;
  }
  return SettingsMenu::onKey(key);
}
