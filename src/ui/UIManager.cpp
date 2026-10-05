#include "UIManager.h"

using namespace std;

#define SCREEN_MARGIN 24.0f

UIManager::UIManager(GLFWwindow *window) : window(window) {
  glfwSetWindowUserPointer(window, this);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCharCallback(window, charCallback);
}

void UIManager::keyCallback(GLFWwindow *window, int key, int, int action,
                            int) {
  // Held down, only Backspace repeats (to delete text)
  if (action != GLFW_PRESS &&
      !(action == GLFW_REPEAT && key == GLFW_KEY_BACKSPACE))
    return;
  UIManager *ui = static_cast<UIManager *>(glfwGetWindowUserPointer(window));
  ui->input.push_back({key, 0u});
}

void UIManager::charCallback(GLFWwindow *window, unsigned int codepoint) {
  UIManager *ui = static_cast<UIManager *>(glfwGetWindowUserPointer(window));
  ui->input.push_back({-1, codepoint});
}

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

void UIManager::placeCentered(UIPanel *panel) {
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  panel->moveTo((width - panel->getWidth()) / 2,
                (height - panel->preferredHeight()) / 2);
}

UIPanel *UIManager::open(UIPanel *panel) {
  placeCentered(panel);
  panels.push_back(unique_ptr<UIPanel>(panel));
  return panel;
}

void UIManager::close(UIPanel *panel) {
  if (isOpen(panel))
    panel->requestClose();
}

bool UIManager::isOpen(const UIPanel *panel) const {
  for (const auto &p : panels)
    if (p.get() == panel)
      return true;
  return false;
}

void UIManager::removeOverlay(const UIOverlay *overlay) {
  for (size_t i = 0; i < overlays.size(); i++)
    if (overlays[i] == overlay) {
      overlays.erase(overlays.begin() + i);
      return;
    }
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

  // Keys pressed and characters typed since the last frame. A handler may
  // open or close panels, so the top panel is looked up again for each one.
  vector<InputEvent> events;
  events.swap(input);
  for (const InputEvent &event : events) {
    int key = event.key;
    UIPanel *top = nullptr;
    for (auto it = panels.rbegin(); it != panels.rend(); ++it)
      if (!(*it)->wantsToClose()) {
        top = it->get();
        break;
      }
    if (key < 0) {
      if (dropChar) // the character of the key that ran a binding
        dropChar = false;
      else if (top)
        top->onChar(event.character);
      else
        for (const CharBinding &binding : charBindings)
          if (binding.character == event.character) {
            binding.action();
            break;
          }
      continue;
    }
    dropChar = false; // another key: what comes now is its character
    if (top) {
      if (!top->onKey(key) && key == GLFW_KEY_ESCAPE)
        top->requestClose();
    } else {
      for (const Binding &binding : bindings)
        if (binding.key() == key) {
          binding.action();
          dropChar = true;
          break;
        }
    }
  }

  // Remove closed panels after the input. A key may close a panel while one
  // of its elements is being dragged, so forget the mouse state as well.
  for (size_t i = 0; i < panels.size();)
    if (panels[i]->wantsToClose()) {
      panels.erase(panels.begin() + i);
      active = nullptr;
      state.hovered = state.active = nullptr;
    } else
      i++;
}

void UIManager::draw() {
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  renderer.begin(width, height);
  for (const UIOverlay *overlay : overlays)
    overlay->draw(renderer, (float)width, (float)height);
  for (const auto &panel : panels)
    if (panel->dimsBackground()) {
      renderer.rect(0, 0, width, height, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
      break;
    }
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
