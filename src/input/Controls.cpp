#include "Controls.h"

#include <GLFW/glfw3.h>
#include <cctype>

#include "Settings.h"

using namespace std;

Controls::Controls() {
  bind(Action::MoveForward, GLFW_KEY_W);
  bind(Action::MoveBack, GLFW_KEY_S);
  bind(Action::MoveLeft, GLFW_KEY_A);
  bind(Action::MoveRight, GLFW_KEY_D);
  bind(Action::Use, GLFW_KEY_E);
  bind(Action::LeaveVehicle, GLFW_KEY_LEFT_SHIFT);
  bind(Action::Quit, GLFW_KEY_X);
  bind(Action::Maps, GLFW_KEY_Z);
  bind(Action::DebugSelect, GLFW_KEY_1);
  bind(Action::DebugPlace, GLFW_KEY_2);
}

const char *Controls::describe(Action action) {
  switch (action) {
  case Action::MoveForward: return "Avanzar";
  case Action::MoveBack: return "Retroceder";
  case Action::MoveLeft: return "Izquierda";
  case Action::MoveRight: return "Derecha";
  case Action::Use: return "Usar objeto / hablar / cerrar";
  case Action::LeaveVehicle: return "Bajar del vehiculo";
  case Action::Quit: return "Salir (en el menu de pausa)";
  case Action::Maps: return "Selector de mapas (debug)";
  case Action::DebugSelect: return "Modo seleccion de objetos (debug)";
  case Action::DebugPlace: return "Modo colocacion de objetos (debug)";
  case Action::Count: break;
  }
  return "?";
}

const char *Controls::id(Action action) {
  switch (action) {
  case Action::MoveForward: return "move_forward";
  case Action::MoveBack: return "move_back";
  case Action::MoveLeft: return "move_left";
  case Action::MoveRight: return "move_right";
  case Action::Use: return "use";
  case Action::LeaveVehicle: return "leave_vehicle";
  case Action::Quit: return "quit";
  case Action::Maps: return "maps";
  case Action::DebugSelect: return "debug_select";
  case Action::DebugPlace: return "debug_place";
  case Action::Count: break;
  }
  return "?";
}

Action Controls::actionFor(int key) const {
  for (int i = 0; i < (int)Action::Count; i++)
    if (keys[i] == key)
      return (Action)i;
  return Action::Count;
}

bool Controls::operator==(const Controls &other) const {
  for (int i = 0; i < (int)Action::Count; i++)
    if (keys[i] != other.keys[i])
      return false;
  return true;
}

bool Controls::isBindable(int key) {
  return key != GLFW_KEY_ESCAPE && key != GLFW_KEY_UNKNOWN && key >= 0 &&
         key <= GLFW_KEY_LAST;
}

void Controls::readFrom(const Settings &settings) {
  // The whole set is checked at once: saved keys may be swapped (W/S)
  const int count = (int)Action::Count;
  int wanted[count];
  bool fromFile[count];
  for (int i = 0; i < count; i++) {
    float saved = settings.getFloat(string("controls.") + id((Action)i), -1.0f);
    fromFile[i] = saved >= 0.0f && isBindable((int)saved);
    wanted[i] = fromFile[i] ? (int)saved : keys[i];
  }
  // Two actions on one key (an edited file): drop the saved ones involved
  for (bool changed = true; changed;) {
    changed = false;
    for (int i = 0; i < count && !changed; i++)
      for (int j = i + 1; j < count && !changed; j++)
        if (wanted[i] == wanted[j]) {
          int drop = fromFile[j] ? j : fromFile[i] ? i : -1;
          if (drop < 0)
            return; // can't be fixed: keep the current keys
          wanted[drop] = keys[drop];
          fromFile[drop] = false;
          changed = true;
        }
  }
  for (int i = 0; i < count; i++)
    keys[i] = wanted[i];
}

void Controls::writeTo(Settings &settings) const {
  for (int i = 0; i < (int)Action::Count; i++)
    settings.setFloat(string("controls.") + id((Action)i), (float)keys[i]);
}

const char *Controls::group(Action action) {
  switch (action) {
  case Action::Use:
  case Action::LeaveVehicle: return "Acciones";
  case Action::Quit:
  case Action::Maps:
  case Action::DebugSelect:
  case Action::DebugPlace: return "Menus";
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
      {"Menus", "Objeto: elegir, mover, girar", "Clic izq./der."},
  };
  return fixed;
}
