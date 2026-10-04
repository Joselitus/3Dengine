#ifndef UI_OVERLAY
#define UI_OVERLAY

#include "UIRenderer.h"

// Something drawn over the whole screen that is not a panel (a crosshair, a
// box of debug data...): it takes no input and doesn't count as an open panel,
// so the game keeps its controls while it is shown. Registered with
// UIManager::addOverlay, it is drawn under the panels and the hint.
class UIOverlay {
public:
  virtual ~UIOverlay() = default;
  // `width` and `height`: the window, in the renderer's pixels
  virtual void draw(UIRenderer &renderer, float width, float height) const = 0;
};

#endif
