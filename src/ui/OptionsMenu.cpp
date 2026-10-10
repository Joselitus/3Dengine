#include "OptionsMenu.h"

#include "AudioMenu.h"
#include "CameraMenu.h"
#include "ControlsMenu.h"
#include "PauseMenu.h"
#include "Settings.h"
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
  add(new UIButton("Audio", [this]() {
    this->context.ui.open(new AudioMenu(this->context));
    requestClose();
  }));
  // The window: it changes at once and is remembered ("video.window")
  if (context.isFullscreen && context.setFullscreen) {
    bool full = context.isFullscreen();
    add(new UIButton(full ? "Pantalla: completa" : "Pantalla: ventana", [this, full]() {
      this->context.setFullscreen(!full);
      this->context.settings.setString("video.window", full ? "maximized" : "fullscreen");
      this->context.settings.save();
      this->context.ui.open(new OptionsMenu(this->context)); // (with the button saying the new one)
      requestClose();
    }));
  }
  add(new UIButton("Volver", [this]() { back(); }));
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
