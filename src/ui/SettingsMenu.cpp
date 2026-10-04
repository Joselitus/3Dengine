#include "SettingsMenu.h"

#include "ConfirmDialog.h"
#include "OptionsMenu.h"
#include "UIButton.h"
#include "UIManager.h"
#include "UIRow.h"
#include "UITextBlock.h"

using namespace std;

SettingsMenu::SettingsMenu(const string &title, float width,
                           const MenuContext &context)
    : UIPanel(title, width, false), context(context) {}

void SettingsMenu::addFooter() {
  // What just happened, or a hint; two lines, messages can be long
  add(new UITextBlock(
      [this]() {
        string text = status.empty() ? hint() : status;
        if (hasUnsavedChanges())
          text += " (cambios sin guardar)";
        return text;
      },
      2, nullptr, UITheme::ACCENT));
  UIRow *buttons = add(new UIRow());
  buttons->add(new UIButton("Por defecto", [this]() {
    resetToDefaults();
    status = "Valores por defecto.";
  }));
  buttons->add(new UIButton("Guardar", [this]() { save(); }));
  buttons->add(new UIButton("Volver", [this]() { leave(); }));
}

void SettingsMenu::save() {
  status = apply() ? "Guardado." : "No se ha podido guardar (ver la consola).";
}

void SettingsMenu::leave() {
  if (!hasUnsavedChanges()) {
    back();
    return;
  }
  context.ui.open(new ConfirmDialog(
      "Cambios sin guardar",
      "Hay cambios sin guardar. Si sales sin guardar, se perderán.",
      {{"Guardar y salir",
        [this]() {
          if (apply())
            back();
          else
            status = "No se ha podido guardar (ver la consola).";
        }},
       {"Salir",
        [this]() {
          discard();
          back();
        }}}));
}

void SettingsMenu::back() {
  context.ui.open(new OptionsMenu(context));
  requestClose();
}

bool SettingsMenu::onKey(int key) {
  if (key != GLFW_KEY_ESCAPE)
    return false;
  leave();
  return true;
}
