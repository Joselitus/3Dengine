#ifndef MENU_CONTEXT
#define MENU_CONTEXT

#include <functional>

class Camera;
class Controls;
class UIManager;

// What the game menus (PauseMenu, OptionsMenu, ControlsMenu) need, passed
// along as they open one another. Everything must outlive the menus. Add
// here what a new menu needs instead of growing every constructor.
struct MenuContext {
  UIManager &ui;
  Camera &camera;
  Controls &controls;
  std::function<void()> quit; // ends the game
};

#endif
