#ifndef INTERACTION_SYSTEM
#define INTERACTION_SYSTEM

#include <vector>

#include "Controller.h"
#include "Interactable.h"
#include "UIManager.h"

// Lets the player use Interactable objects. Each frame it looks for the
// closest one in range of the player and shows a hint; E opens its interface
// (the Controller is paused and the cursor freed) and E, Esc or the panel's
// close button close it again.
class InteractionSystem {
private:
  GLFWwindow *window;
  UIManager *ui;
  Controller *controller;
  std::vector<Interactable *> targets; // not owned
  Interactable *inUse = nullptr;       // whose panel is open
  bool useWasDown = false, escapeWasDown = false;

  Interactable *closest(const glm::vec3 &player) const;
  void open(Interactable *target);
  void close();

public:
  InteractionSystem(GLFWwindow *window, UIManager *ui, Controller *controller);

  void add(Interactable *target) { targets.push_back(target); }
  void update(const glm::vec3 &playerPosition);
};

#endif
