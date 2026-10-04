#include "UIManager.h"

using namespace std;

#define SCREEN_MARGIN 24.0f

UIManager::UIManager(GLFWwindow *window) : window(window) {}

UIPanel *UIManager::open(Interactable &target) {
  UIPanel *panel = new UIPanel(target.getInteractionName());
  target.buildInterface(*panel);
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  panel->moveTo(width - panel->getWidth() - SCREEN_MARGIN,
                (height - panel->preferredHeight()) / 2);
  panels.push_back(unique_ptr<UIPanel>(panel));
  return panel;
}

void UIManager::closeAll() {
  panels.clear();
  active = nullptr;
  state.hovered = state.active = nullptr;
}

void UIManager::update() {
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  state.mouseX = (float)x;
  state.mouseY = (float)y;
  bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

  // Top-most panel under the cursor, and the element in it. Elements that
  // don't take the mouse (labels, rows) hand the click to their panel.
  UIElement *hit = nullptr;
  UIPanel *hitPanel = nullptr;
  for (auto it = panels.rbegin(); it != panels.rend() && !hit; ++it) {
    hit = (*it)->elementAt(state.mouseX, state.mouseY);
    hitPanel = it->get();
  }
  if (hit && !hit->isInteractive())
    hit = hitPanel;
  state.hovered = hit;

  if (down && !buttonWasDown && hit) {
    active = hit;
    active->onPress(state.mouseX, state.mouseY);
    // Bring the clicked panel to the front
    for (size_t i = 0; i < panels.size(); i++)
      if (panels[i].get() == hitPanel) {
        unique_ptr<UIPanel> front = move(panels[i]);
        panels.erase(panels.begin() + i);
        panels.push_back(move(front));
        break;
      }
  } else if (down && active) {
    active->onDrag(state.mouseX, state.mouseY);
  } else if (!down && active) {
    active->onRelease(state.mouseX, state.mouseY,
                      active->getRect().contains(state.mouseX, state.mouseY));
    active = nullptr;
  }
  buttonWasDown = down;
  state.active = active;

  // Remove closed panels after the input, when none of their elements is
  // in use (a panel closes on the release of its close button)
  for (size_t i = 0; i < panels.size();)
    if (panels[i]->wantsToClose()) {
      panels.erase(panels.begin() + i);
      state.hovered = nullptr;
    } else
      i++;
}

void UIManager::draw() {
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  renderer.begin(width, height);
  for (const auto &panel : panels)
    panel->draw(renderer, state);
  if (!hint.empty()) {
    float w = UIRenderer::textWidth(hint) + 2 * UITheme::PADDING;
    float h = UIRenderer::textHeight() + UITheme::PADDING;
    float x = (width - w) / 2, y = height - h - SCREEN_MARGIN;
    renderer.rect(x, y, w, h, UITheme::PANEL);
    renderer.frame(x, y, w, h, 1.0f, UITheme::BORDER);
    renderer.text(x + UITheme::PADDING, y + UITheme::PADDING / 2 + 2.0f, hint,
                  UITheme::TEXT);
  }
  renderer.end();
}
