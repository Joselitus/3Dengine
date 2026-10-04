#ifndef INTERACTION_SYSTEM
#define INTERACTION_SYSTEM

#include <vector>

#include "Controls.h"
#include "Interactable.h"
#include "UIManager.h"

// Lets the player use Interactable objects. Each frame it looks for the
// closest one in range of the player and shows a hint; the Use key (E by
// default, see Controls) opens its interface and closes it again (so do Esc and the close button, through the
// UIManager). It only opens one when no other panel (e.g. a menu) is open.
// Pausing the player's controls while a panel is open is up to the game loop.
class InteractionSystem {
private:
  GLFWwindow *window;
  UIManager *ui;
  const Controls &controls;
  std::vector<Interactable *> targets; // not owned
  UIPanel *panel = nullptr;            // the open one, if any (not owned)
  Interactable *inUse = nullptr;       // whose panel it is
  bool useWasDown = false;

  Interactable *closest(const glm::vec3 &player) const;

public:
  InteractionSystem(GLFWwindow *window, UIManager *ui,
                    const Controls &controls);

  void add(Interactable *target) { targets.push_back(target); }
  // Forget every target (e.g. their map is about to be destroyed); close
  // their panel first (UIManager::closeAll). onInterfaceClosed is not called:
  // the target may be gone already.
  void clear() {
    targets.clear();
    panel = nullptr;
    inUse = nullptr;
  }
  // enabled = false: nothing can be used and no prompt is shown
  void update(const glm::vec3 &playerPosition, bool enabled = true);
};

#endif
