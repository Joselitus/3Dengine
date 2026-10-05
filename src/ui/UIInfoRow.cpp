#include "UIInfoRow.h"

using namespace std;

UIInfoRow::UIInfoRow(const string &text, const string &value)
    : text(text), value([value]() { return value; }) {}

UIInfoRow::UIInfoRow(const string &text, function<string()> value,
                     function<void()> onClick)
    : text(text), value(value), onClick(onClick) {}

#define PADDING 1.0f // around the text
#define HIGHLIGHT 2.0f // the highlight reaches this far into the gap between rows

float UIInfoRow::preferredHeight() const {
  return UIRenderer::textHeight() + 2 * PADDING;
}

void UIInfoRow::draw(UIRenderer &renderer, const UIState &state) const {
  if (onClick && (state.hovered == this || state.active == this))
    renderer.rect(rect.x - PADDING - HIGHLIGHT, rect.y - HIGHLIGHT,
                  rect.w + 2 * (PADDING + HIGHLIGHT), rect.h + 2 * HIGHLIGHT,
                  UITheme::HOVER);
  string shown = value();
  float y = rect.y + PADDING;
  renderer.text(rect.x, y, text, UITheme::TEXT);
  renderer.text(rect.x + rect.w - UIRenderer::textWidth(shown), y, shown,
                UITheme::ACCENT);
}

void UIInfoRow::onRelease(float, float, bool inside) {
  if (inside && onClick)
    onClick();
}
