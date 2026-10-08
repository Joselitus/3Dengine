#include "ConnectMenu.h"

#include <GLFW/glfw3.h>

#include "UIButton.h"
#include "UILabel.h"
#include "UITextBlock.h"
#include "UITextField.h"

using namespace std;

ConnectMenu::ConnectMenu(const string &addressText, const string &nameText,
                         function<void(const string &, const string &)> connect,
                         function<void()> quit)
    : UIPanel("Conectar al servidor", 460.0f, false), connect(connect), quit(quit) {
  add(new UILabel("Servidor (IP o nombre, :puerto opcional)", UITheme::MUTED));
  address = add(new UITextField(120));
  address->setText(addressText);
  add(new UILabel("Tu nombre", UITheme::MUTED));
  name = add(new UITextField(24));
  name->setText(nameText);
  setFocus(address);
  add(new UITextBlock([this]() { return status; }, 2));
  add(new UIButton([this]() { return busy ? string("Conectando...") : string("Conectar"); },
                   [this]() { submit(); }));
  add(new UIButton("Salir", [this]() { this->quit(); }));
}

const string &ConnectMenu::getAddress() const { return address->getText(); }
const string &ConnectMenu::getName() const { return name->getText(); }

void ConnectMenu::setFocus(UITextField *field) {
  focus = field;
  address->setFocused(field == address);
  name->setFocused(field == name);
}

void ConnectMenu::submit() {
  if (busy)
    return;
  if (address->getText().empty()) {
    status = "Escribe la direccion del servidor";
    return;
  }
  connect(address->getText(), name->getText());
}

bool ConnectMenu::onChar(unsigned int codepoint) { return focus->add(codepoint); }

bool ConnectMenu::onKey(int key) {
  switch (key) {
  case GLFW_KEY_ENTER:
  case GLFW_KEY_KP_ENTER:
    submit();
    return true;
  case GLFW_KEY_BACKSPACE:
    focus->backspace();
    return true;
  case GLFW_KEY_TAB:
  case GLFW_KEY_DOWN:
  case GLFW_KEY_UP:
    setFocus(focus == address ? name : address);
    return true;
  case GLFW_KEY_ESCAPE:
    quit();
    return true;
  }
  return false;
}
