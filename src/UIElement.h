#ifndef UI_ELEMENT
#define UI_ELEMENT

#include <glm/glm.hpp>

#include "UIRenderer.h"

// Rectangle in window pixels, (x, y) is the top left corner
struct UIRect {
  float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
  bool contains(float px, float py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

// Colours and sizes shared by every element
namespace UITheme {
const glm::vec4 PANEL(0.06f, 0.08f, 0.14f, 0.88f);
const glm::vec4 TITLE_BAR(0.13f, 0.18f, 0.32f, 0.95f);
const glm::vec4 BORDER(0.30f, 0.38f, 0.60f, 1.0f);
const glm::vec4 TEXT(0.88f, 0.91f, 0.97f, 1.0f);
const glm::vec4 MUTED(0.58f, 0.63f, 0.76f, 1.0f);
const glm::vec4 CONTROL(0.16f, 0.20f, 0.33f, 1.0f);
const glm::vec4 HOVER(0.24f, 0.30f, 0.50f, 1.0f);
const glm::vec4 ACCENT(0.98f, 0.55f, 0.18f, 1.0f);
const float PADDING = 10.0f;
const float SPACING = 6.0f;
} // namespace UITheme

class UIElement;

// What the manager knows about the mouse when an element is drawn
struct UIState {
  const UIElement *hovered = nullptr; // under the cursor
  const UIElement *active = nullptr;  // being pressed or dragged
  float mouseX = 0.0f, mouseY = 0.0f; // cursor, in window pixels
};

// Base of every piece of the interface. A parent gives each child its place
// with layout(); the child picks its own height (preferredHeight). Mouse
// input arrives through the UIManager: elementAt() finds the element under
// the cursor and, if it is interactive, it gets onPress, then onDrag while
// the button is held, then onRelease.
class UIElement {
protected:
  UIRect rect;

public:
  virtual ~UIElement() = default;

  virtual float preferredHeight() const = 0;
  // Places the element at (x, y) with the given width
  virtual void layout(float x, float y, float width) {
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = preferredHeight();
  }
  virtual void draw(UIRenderer &renderer, const UIState &state) const = 0;

  // Deepest element under (x, y), or nullptr. Containers search children.
  virtual UIElement *elementAt(float x, float y) {
    return rect.contains(x, y) ? this : nullptr;
  }
  // Whether the element reacts to the mouse (shows hover, takes presses)
  virtual bool isInteractive() const { return false; }
  virtual void onPress(float x, float y) {}
  virtual void onDrag(float x, float y) {}
  // `inside`: the button was released over the element
  virtual void onRelease(float x, float y, bool inside) {}

  const UIRect &getRect() const { return rect; }
};

#endif
