#ifndef UI_MANAGER
#define UI_MANAGER

#include <memory>
#include <string>
#include <vector>

#include "myopengl.h"
#include "Interactable.h"
#include "UIPanel.h"
#include "UIRenderer.h"

// Owns the open panels, feeds them the mouse and draws them over the scene.
// Also shows a one-line hint at the bottom of the screen (e.g. "E: use").
// Call update() once per frame (input) and draw() after the 3D scene.
class UIManager {
private:
  GLFWwindow *window;
  UIRenderer renderer;
  std::vector<std::unique_ptr<UIPanel>> panels; // last = on top
  std::string hint;

  UIState state;              // what elements see when drawn
  UIElement *active = nullptr; // receives drag/release until the button is up
  bool buttonWasDown = false;

public:
  explicit UIManager(GLFWwindow *window);

  // Opens a panel for `target` (built by the target itself), centred
  // vertically at the right of the screen, leaving the view in front of the
  // player (where the object usually is) clear
  UIPanel *open(Interactable &target);
  void closeAll();
  bool hasPanels() const { return !panels.empty(); }

  void setHint(const std::string &text) { hint = text; }

  void update();
  void draw();
};

#endif
