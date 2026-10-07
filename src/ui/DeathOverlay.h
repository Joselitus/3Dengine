#ifndef DEATH_OVERLAY
#define DEATH_OVERLAY

#include "UIOverlay.h"

// The colour that tints the whole screen when the player dies (red) or is abducted (white, then
// black): `amount` (0 = nothing, 1 = fully that colour) is set by the main loop as it goes on.
// Registered with UIManager::addOverlay.
class DeathOverlay : public UIOverlay {
  float amount = 0.0f;
  glm::vec3 color = glm::vec3(0.7f, 0.0f, 0.02f);

public:
  void setAmount(float amount) { this->amount = amount; }
  void setColor(const glm::vec3 &c) { color = c; }
  void draw(UIRenderer &renderer, float width, float height) const override;
};

#endif
