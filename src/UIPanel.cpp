#include "UIPanel.h"

using namespace std;

#define TITLE_HEIGHT 30.0f

UIPanel::UIPanel(const string &title, float width)
    : title(title), width(width) {}

void UIPanel::moveTo(float x, float y) { layout(x, y, width); }

UIRect UIPanel::titleBar() const {
  UIRect bar = rect;
  bar.h = TITLE_HEIGHT;
  return bar;
}

UIRect UIPanel::closeButton() const {
  UIRect button;
  button.w = button.h = TITLE_HEIGHT - 8.0f;
  button.x = rect.x + rect.w - button.w - 4.0f;
  button.y = rect.y + 4.0f;
  return button;
}

float UIPanel::preferredHeight() const {
  float height = TITLE_HEIGHT + UITheme::PADDING;
  for (const auto &child : children)
    height += child->preferredHeight() + UITheme::SPACING;
  return height - UITheme::SPACING + UITheme::PADDING;
}

void UIPanel::layout(float x, float y, float width) {
  UIElement::layout(x, y, width);
  float cursor = y + TITLE_HEIGHT + UITheme::PADDING;
  float inner = width - 2 * UITheme::PADDING;
  for (const auto &child : children) {
    child->layout(x + UITheme::PADDING, cursor, inner);
    cursor += child->preferredHeight() + UITheme::SPACING;
  }
}

void UIPanel::draw(UIRenderer &renderer, const UIState &state) const {
  renderer.rect(rect.x, rect.y, rect.w, rect.h, UITheme::PANEL);
  UIRect bar = titleBar();
  renderer.rect(bar.x, bar.y, bar.w, bar.h, UITheme::TITLE_BAR);
  renderer.text(bar.x + UITheme::PADDING,
                bar.y + (bar.h - UIRenderer::textHeight()) / 2 + 2.0f, title,
                UITheme::TEXT);

  UIRect close = closeButton();
  bool overClose = state.hovered == this && close.contains(state.mouseX, state.mouseY);
  renderer.rect(close.x, close.y, close.w, close.h,
                overClose ? UITheme::ACCENT : UITheme::CONTROL);
  renderer.text(close.x + (close.w - UIRenderer::textWidth("X")) / 2,
                close.y + 3.0f, "X", UITheme::TEXT);

  renderer.frame(rect.x, rect.y, rect.w, rect.h, 1.0f, UITheme::BORDER);
  UIContainer::draw(renderer, state);
}

void UIPanel::onPress(float x, float y) {
  if (closeButton().contains(x, y))
    return; // closes on release, like a button
  if (titleBar().contains(x, y)) {
    dragging = true;
    grabX = x - rect.x;
    grabY = y - rect.y;
  }
}

void UIPanel::onDrag(float x, float y) {
  if (dragging)
    moveTo(x - grabX, y - grabY);
}

void UIPanel::onRelease(float x, float y, bool) {
  if (!dragging && closeButton().contains(x, y))
    closeRequested = true;
  dragging = false;
}
