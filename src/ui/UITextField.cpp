#include "UITextField.h"

#include <cmath>

#include <GLFW/glfw3.h>

using namespace std;

#define PADDING 6.0f
#define BLINK 0.5 // seconds the cursor is shown, and hidden

void UITextField::setText(const string &value) {
  text.clear();
  for (char c : value)
    add((unsigned char)c);
}

bool UITextField::add(unsigned int codepoint) {
  if (codepoint < 32 || codepoint > 126 || text.size() >= maxLength)
    return false;
  text += (char)codepoint;
  return true;
}

void UITextField::backspace() {
  if (!text.empty())
    text.pop_back();
}

float UITextField::preferredHeight() const {
  return UIRenderer::textHeight() + 2 * PADDING;
}

void UITextField::draw(UIRenderer &renderer, const UIState &) const {
  renderer.rect(rect.x, rect.y, rect.w, rect.h, UITheme::CONTROL);
  renderer.frame(rect.x, rect.y, rect.w, rect.h, 1.0f, focused ? UITheme::ACCENT : UITheme::BORDER);
  // If it is longer than the box, its end (where the cursor is) is shown
  float room = rect.w - 2 * PADDING - 4.0f;
  size_t first = 0;
  while (first < text.size() && UIRenderer::textWidth(text.substr(first)) > room)
    first++;
  string shown = text.substr(first);
  float x = rect.x + PADDING, y = rect.y + PADDING;
  renderer.text(x, y, shown, UITheme::TEXT);
  if (focused && std::fmod(glfwGetTime(), 2 * BLINK) < BLINK)
    renderer.rect(x + UIRenderer::textWidth(shown) + 1.0f, y - 1.0f, 2.0f,
                  UIRenderer::textHeight() + 2.0f, UITheme::TEXT);
}
