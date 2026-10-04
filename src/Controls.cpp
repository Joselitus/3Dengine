#include "Controls.h"

#include <GLFW/glfw3.h>
#include <cctype>

using namespace std;

Controls::Controls() {
  bind(Action::MoveForward, GLFW_KEY_W);
  bind(Action::MoveBack, GLFW_KEY_S);
  bind(Action::MoveLeft, GLFW_KEY_A);
  bind(Action::MoveRight, GLFW_KEY_D);
  bind(Action::MoveUp, GLFW_KEY_SPACE);
  bind(Action::MoveDown, GLFW_KEY_LEFT_SHIFT);
  bind(Action::Use, GLFW_KEY_E);
  bind(Action::Quit, GLFW_KEY_X);
  bind(Action::Maps, GLFW_KEY_Z);
}

const char *Controls::describe(Action action) {
  switch (action) {
  case Action::MoveForward: return "Avanzar";
  case Action::MoveBack: return "Retroceder";
  case Action::MoveLeft: return "Izquierda";
  case Action::MoveRight: return "Derecha";
  case Action::MoveUp: return "Subir (sin gravedad)";
  case Action::MoveDown: return "Bajar (sin gravedad)";
  case Action::Use: return "Usar objeto / hablar / cerrar";
  case Action::Quit: return "Salir (en el menu de pausa)";
  case Action::Maps: return "Selector de mapas (debug)";
  case Action::Count: break;
  }
  return "?";
}

const char *Controls::group(Action action) {
  switch (action) {
  case Action::Use: return "Acciones";
  case Action::Quit:
  case Action::Maps: return "Menus";
  default: return "Movimiento";
  }
}

string Controls::keyName(int key) {
  switch (key) {
  case GLFW_KEY_SPACE: return "Espacio";
  case GLFW_KEY_LEFT_SHIFT: return "Mayus izq.";
  case GLFW_KEY_RIGHT_SHIFT: return "Mayus der.";
  case GLFW_KEY_LEFT_CONTROL: return "Ctrl izq.";
  case GLFW_KEY_RIGHT_CONTROL: return "Ctrl der.";
  case GLFW_KEY_LEFT_ALT: return "Alt izq.";
  case GLFW_KEY_TAB: return "Tab";
  case GLFW_KEY_ENTER: return "Intro";
  case GLFW_KEY_ESCAPE: return "Esc";
  case GLFW_KEY_BACKSPACE: return "Retroceso";
  case GLFW_KEY_UP: return "Flecha arriba";
  case GLFW_KEY_DOWN: return "Flecha abajo";
  case GLFW_KEY_LEFT: return "Flecha izq.";
  case GLFW_KEY_RIGHT: return "Flecha der.";
  }
  // Printable keys: the character on the user's keyboard layout
  const char *name = glfwGetKeyName(key, 0);
  if (name && name[0] && !(name[0] & 0x80) && !name[1])
    return string(1, (char)toupper(name[0]));
  if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
    return "F" + to_string(key - GLFW_KEY_F1 + 1);
  return "Tecla " + to_string(key);
}

const vector<Controls::Fixed> &Controls::fixedControls() {
  static const vector<Fixed> fixed = {
      {"Camara", "Mirar alrededor", "Raton"},
      {"Menus", "Menu de pausa / atras / cerrar panel", "Esc"},
      {"Menus", "Botones y deslizadores", "Clic izquierdo"},
  };
  return fixed;
}
