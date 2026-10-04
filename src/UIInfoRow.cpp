#include "UIInfoRow.h"

using namespace std;

UIInfoRow::UIInfoRow(const string &text, const string &value)
    : text(text), value([value]() { return value; }) {}

UIInfoRow::UIInfoRow(const string &text, function<string()> value)
    : text(text), value(value) {}

float UIInfoRow::preferredHeight() const { return UIRenderer::textHeight(); }

void UIInfoRow::draw(UIRenderer &renderer, const UIState &) const {
  string shown = value();
  renderer.text(rect.x, rect.y, text, UITheme::TEXT);
  renderer.text(rect.x + rect.w - UIRenderer::textWidth(shown), rect.y, shown,
                UITheme::ACCENT);
}
