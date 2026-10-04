#include "UIRow.h"

#include <algorithm>

float UIRow::preferredHeight() const {
  float height = 0.0f;
  for (const auto &child : children)
    height = std::max(height, child->preferredHeight());
  return height;
}

void UIRow::layout(float x, float y, float width) {
  UIElement::layout(x, y, width);
  if (children.empty())
    return;
  float gaps = UITheme::SPACING * (children.size() - 1);
  float childWidth = (width - gaps) / children.size();
  for (size_t i = 0; i < children.size(); i++)
    children[i]->layout(x + i * (childWidth + UITheme::SPACING), y,
                        childWidth);
}
