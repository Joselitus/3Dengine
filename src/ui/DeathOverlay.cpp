#include "DeathOverlay.h"

void DeathOverlay::draw(UIRenderer &renderer, float width, float height) const {
  if (amount > 0.0f)
    renderer.rect(0.0f, 0.0f, width, height, glm::vec4(0.7f, 0.0f, 0.02f, amount));
}
