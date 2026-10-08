#include "CrosshairOverlay.h"

#include "UIRenderer.h"

void CrosshairOverlay::draw(UIRenderer &renderer, float width, float height) const {
  if (!shown)
    return;
  const float GAP = 5.0f, LENGTH = 9.0f, THICK = 2.0f;
  const glm::vec4 shade(0.0f, 0.0f, 0.0f, 0.5f), color(0.55f, 1.0f, 0.6f, 0.9f);
  float cx = width / 2.0f, cy = height / 2.0f;
  // four arms round a gap and a dot in the middle, each over a dark outline
  for (int pass = 0; pass < 2; pass++) {
    float o = pass == 0 ? 1.0f : 0.0f;
    const glm::vec4 &c = pass == 0 ? shade : color;
    renderer.rect(cx - GAP - LENGTH - o, cy - THICK / 2 - o, LENGTH + 2 * o, THICK + 2 * o, c);
    renderer.rect(cx + GAP - o, cy - THICK / 2 - o, LENGTH + 2 * o, THICK + 2 * o, c);
    renderer.rect(cx - THICK / 2 - o, cy - GAP - LENGTH - o, THICK + 2 * o, LENGTH + 2 * o, c);
    renderer.rect(cx - THICK / 2 - o, cy + GAP - o, THICK + 2 * o, LENGTH + 2 * o, c);
    renderer.rect(cx - 1.0f - o, cy - 1.0f - o, 2.0f + 2 * o, 2.0f + 2 * o, c);
  }
}
