#include "UIContainer.h"

void UIContainer::draw(UIRenderer &renderer, const UIState &state) const {
  for (const auto &child : children)
    child->draw(renderer, state);
}

UIElement *UIContainer::elementAt(float x, float y) {
  if (!rect.contains(x, y))
    return nullptr;
  for (const auto &child : children)
    if (UIElement *hit = child->elementAt(x, y))
      return hit;
  return this;
}
