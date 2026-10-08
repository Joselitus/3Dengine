#ifndef CROSSHAIR_OVERLAY
#define CROSSHAIR_OVERLAY

#include "UIOverlay.h"

// A small green cross in the middle of the screen, while the player aims a gun (Bob's ship's ray
// gun: the main loop shows it with GameStage::playerAiming). Registered with UIManager::addOverlay.
class CrosshairOverlay : public UIOverlay {
  bool shown = false;

public:
  void setShown(bool show) { shown = show; }
  void draw(UIRenderer &renderer, float width, float height) const override;
};

#endif
