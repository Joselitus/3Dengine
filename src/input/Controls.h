#ifndef CONTROLS
#define CONTROLS

#include <string>
#include <vector>

class Settings;

// Every game action that is triggered by a key.
enum class Action {
  MoveForward,
  MoveBack,
  MoveLeft,
  MoveRight,
  Use,      // open/close the panel of the object in front (InteractionSystem)
  LeaveVehicle, // get out of the vehicle being driven (Left Shift)
  Quit,     // in the pause menu
  Maps,     // debug map selector
  DebugSelect, // debug: select objects and show their data (DebugSelector)
  DebugPlace,  // debug: move the selected object where the camera points
  Count     // number of actions, not an action
};

// The key bound to each Action: the single place the game reads its keys
// from (Controller, InteractionSystem, menus, UIManager key bindings), and
// what the ControlsMenu lists and lets the player rebind. Everything reads
// it when it needs a key, so a rebinding applies at once. Saved in the
// Settings as "controls.<id> = <GLFW key code>".
//
// Esc is not an Action: it is the interface's fixed "back" key (closes the
// top panel, opens the pause menu), handled by UIManager. The mouse is not
// rebindable either; fixedControls() lists both for the help screen.
class Controls {
private:
  int keys[(int)Action::Count];

public:
  Controls(); // default keys (WASD, E, Left Shift, X, Z, 1, 2)

  int key(Action action) const { return keys[(int)action]; }
  void bind(Action action, int key) { keys[(int)action] = key; }
  // The action bound to `key`, or Action::Count if none
  Action actionFor(int key) const;
  bool operator==(const Controls &other) const;
  bool operator!=(const Controls &other) const { return !(*this == other); }

  // Whether `key` may be bound to an action (Esc is reserved, see below)
  static bool isBindable(int key);
  // Keys saved in `settings`; a missing, invalid or repeated one keeps its
  // current key
  void readFrom(const Settings &settings);
  void writeTo(Settings &settings) const; // not saved to disk: Settings::save
  // Name of the key bound to `action`, for the interface
  std::string keyName(Action action) const { return keyName(key(action)); }

  // What the action does, e.g. "Avanzar"
  static const char *describe(Action action);
  // Stable name in the settings file, e.g. "move_forward"
  static const char *id(Action action);
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
