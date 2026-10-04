#ifndef MENU_CONTEXT
#define MENU_CONTEXT

#include <functional>

class Camera;
class Controls;
class Settings;
class SoundEngine;
class UIManager;

// What the game menus (PauseMenu, OptionsMenu, ControlsMenu) need, passed
// along as they open one another. Everything must outlive the menus. Add
// here what a new menu needs instead of growing every constructor.
struct MenuContext {
  UIManager &ui;
  Camera &camera;
  Controls &controls;
  Settings &settings;         // saved between sessions (OptionsMenu)
  SoundEngine &sound;         // master volume (AudioMenu)
  std::function<void()> quit; // ends the game
};

#endif
