#ifndef CONTROLS
#define CONTROLS

#include <string>
#include <vector>

// Every game action that is triggered by a key.
enum class Action {
  MoveForward,
  MoveBack,
  MoveLeft,
  MoveRight,
  MoveUp,   // only without gravity
  MoveDown, // only without gravity
  Use,      // open/close the panel of the object in front (InteractionSystem)
  Quit,     // in the pause menu
  Maps,     // debug map selector
  Count     // number of actions, not an action
};

// The key bound to each Action: the single place the game reads its keys
// from (Controller, InteractionSystem, menus, test.cpp), and what the
// ControlsMenu lists. Rebinding later only needs bind() plus a UI for it;
// nothing else hardcodes these keys.
//
// Esc is not an Action: it is the interface's fixed "back" key (closes the
// top panel, opens the pause menu), handled by UIManager. The mouse is not
// rebindable either; fixedControls() lists both for the help screen.
class Controls {
private:
  int keys[(int)Action::Count];

public:
  Controls(); // default keys (WASD, Space/Left Shift, E, X, Z)

  int key(Action action) const { return keys[(int)action]; }
  void bind(Action action, int key) { keys[(int)action] = key; }
  // Name of the key bound to `action`, for the interface
  std::string keyName(Action action) const { return keyName(key(action)); }

  // What the action does, e.g. "Avanzar" (ASCII: see stb_easy_font)
  static const char *describe(Action action);
  // Group it is listed under: "Movimiento", "Acciones", "Menus"
  static const char *group(Action action);
  // Readable name of a GLFW key, e.g. "W", "Espacio", "Mayus izq."
  static std::string keyName(int key);

  struct Fixed {
    const char *group, *description, *input;
  };
  // Inputs that are not Actions (mouse, Esc), for the help screen
  static const std::vector<Fixed> &fixedControls();
};

#endif
