#include "UIButton.h"

using namespace std;

UIButton::UIButton(const string &text, function<void()> action)
    : text([text]() { return text; }), action(action) {}

UIButton::UIButton(function<string()> text, function<void()> action)
    : text(text), action(action) {}

float UIButton::preferredHeight() const {
  return UIRenderer::textHeight() + 12.0f;
}

void UIButton::draw(UIRenderer &renderer, const UIState &state) const {
  bool pressed = state.active == this && state.hovered == this;
  glm::vec4 fill = pressed ? UITheme::ACCENT
                   : state.hovered == this ? UITheme::HOVER
                                           : UITheme::CONTROL;
  renderer.rect(rect.x, rect.y, rect.w, rect.h, fill);
  renderer.frame(rect.x, rect.y, rect.w, rect.h, 1.0f, UITheme::BORDER);
  string shown = text();
  float width = UIRenderer::textWidth(shown);
  renderer.text(rect.x + (rect.w - width) / 2, rect.y + 6.0f, shown,
                UITheme::TEXT);
}

void UIButton::onRelease(float, float, bool inside) {
  if (inside && action)
    action();
}
