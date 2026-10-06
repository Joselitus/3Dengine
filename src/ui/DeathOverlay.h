#ifndef DEATH_OVERLAY
#define DEATH_OVERLAY

#include "UIOverlay.h"

// The red that tints the whole screen when the player dies: `amount` (0 = nothing, 1 = fully red) is
// set by the main loop as the death goes on. Registered with UIManager::addOverlay.
class DeathOverlay : public UIOverlay {
  float amount = 0.0f;

public:
  void setAmount(float amount) { this->amount = amount; }
  void draw(UIRenderer &renderer, float width, float height) const override;
};

#endif
