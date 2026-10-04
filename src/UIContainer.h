#ifndef UI_CONTAINER
#define UI_CONTAINER

#include <memory>
#include <vector>

#include "UIElement.h"

// An element made of other elements, which it owns. Subclasses decide where
// the children go (layout); drawing and mouse lookup recurse into them.
class UIContainer : public UIElement {
protected:
  std::vector<std::unique_ptr<UIElement>> children;

public:
  // Adds a child and returns it, already typed, so it can be configured:
  //   panel.add(new UILabel("Hello"));
  template <typename T> T *add(T *child) {
    children.push_back(std::unique_ptr<UIElement>(child));
    return child;
  }

  void draw(UIRenderer &renderer, const UIState &state) const override;
  UIElement *elementAt(float x, float y) override;
};

#endif
